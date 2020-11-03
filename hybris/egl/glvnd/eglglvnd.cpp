#include <string>
#include <cstring>

#include <EGL/egl.h>
#include <glvnd/libeglabi.h>

#include "eglhybris.h"
#include "eglglvnd.h"
#include "egldispatchstubs.h"

static const __EGLapiExports *__eglGLVNDApiExports = NULL;

/* Libhybris doesn't support EGL 1.5 and eglGetPlatformDisplay() variant. Doing
 * so would require some undertaking changes in libhybris EGL's ws system. So,
 * we would advertise no platform support in client extensions.
 * 
 * According to Mesa, the normal eglQueryString shouldn't include platform
 * extensions, so we'll strip it. Platform extensions are queried via glvnd's
 * getVendorString(). We won't support any, so we'll return NULL.
 */

static std::string clientExtensionNoPlatform()
{
    std::string clientExts;
    const char * clientExtsOrigCStr = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    if (!clientExtsOrigCStr || clientExtsOrigCStr[0] == '\0')
        return clientExts;

    const std::string clientExtsOrig(clientExtsOrigCStr);

    std::size_t start = 0, end;
    do {
        end = clientExtsOrig.find(' ', start);
        // end might be string::npos, but removing start from ::npos shouldn't
        // cause any issue.
        auto ext = clientExtsOrig.substr(start, end - start);
        if (ext.find("_platform_") == std::string::npos) {
            if (clientExts.empty()) {
                clientExts = ext;
            } else {
                clientExts += ' ';
                clientExts += ext;
            }
        }

        start = end + 1; // Skip the space.
                         // If end is npos then this value won't be used anyway.
    } while (end != std::string::npos);

    return clientExts;
}

static const char * EGLAPIENTRY
__eglGLVNDQueryString(EGLDisplay dpy, EGLenum name)
{
    if (dpy == EGL_NO_DISPLAY && name == EGL_EXTENSIONS) {
        // Rely on C++11's static initialization guarantee.
        static const std::string clientExts = clientExtensionNoPlatform();

        // We can do this because if Android's EGL support client
        // extensions, it will at least have EGL_EXT_client_extensions.
        if (clientExts.empty())
            return NULL;
        else
            return clientExts.c_str();
    }

    // Use libhybris's normal eglQueryString() otherwise.
    return eglQueryString(dpy, name);
}

static const char *
__eglGLVNDGetVendorString(int name)
{
    if (name == __EGL_VENDOR_STRING_PLATFORM_EXTENSIONS)
        // TODO: do something if we eventually support eglGetPlatformDisplay().
        return NULL;

    return NULL;
}

static EGLDisplay
__eglGLVNDGetPlatformDisplay(EGLenum platform, void *native_display,
        const EGLAttrib * /* attrib_list */)
{
    if (platform != EGL_NONE) {
        __eglHybrisSetError(EGL_BAD_PARAMETER);
        return EGL_NO_DISPLAY;
    }

    return eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(native_display));
}

static EGLBoolean __eglGLVNDgetSupportsAPI (EGLenum api)
{
    return api == EGL_OPENGL_ES_API;
}

static void *
__eglGLVNDGetProcAddress(const char *procName)
{
    if (strcmp(procName ,"eglQueryString") == 0)
        return (void *) __eglGLVNDQueryString;
    else
        return (void *) eglGetProcAddress(procName);
}

/* For glvnd-supported libEGL, every symbols except for the entry point,
 * __egl_Main(), must be hidden. In our case, we pass -fvisibility=hidden
 * flag, then explicitly set this function visible.
 */

__attribute__ ((visibility ("default")))
EGLBoolean
__egl_Main(uint32_t version, const __EGLapiExports *exports,
     __EGLvendorInfo *vendor, __EGLapiImports *imports)
{
    if (EGL_VENDOR_ABI_GET_MAJOR_VERSION(version) !=
        EGL_VENDOR_ABI_MAJOR_VERSION)
       return EGL_FALSE;

    __eglGLVNDApiExports = exports;
    __eglInitDispatchStubs(exports);

    imports->getPlatformDisplay = __eglGLVNDGetPlatformDisplay;
    imports->getSupportsAPI = __eglGLVNDgetSupportsAPI;
    imports->getVendorString = __eglGLVNDGetVendorString;
    imports->getProcAddress = __eglGLVNDGetProcAddress;
    imports->getDispatchAddress = __eglDispatchFindDispatchFunction;
    imports->setDispatchIndex = __eglSetDispatchIndex;

    return EGL_TRUE;
}
