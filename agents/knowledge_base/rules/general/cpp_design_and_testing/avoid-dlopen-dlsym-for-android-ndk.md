---
name: general/avoid-dlopen-dlsym-for-android-ndk
rubric: design
severity: should_fix
---

**Rule.** Do not use dlopen, dlsym, or custom dynamic-loading wrapper tables to
access Android NDK APIs. Call standard NDK functions directly because Chromium
targets Android Q+ (minSdkVersion=29), and rely on NDK weak symbols
(__ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__) with runtime nullptr checks rather
than dlsym for APIs introduced in newer Android versions.

**Why.** Chromium sets minSdkVersion=29 (Android 10), ensuring APIs like
AHardwareBuffer (API 26) are unconditionally available at runtime without
dynamic lookup. For APIs introduced after API 29, Chromium compiles with
__ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__, allowing direct function calls guarded
by a nullptr check instead of cumbersome and error-prone dlopen/dlsym lookup
tables.

## Examples

Bad:

```cpp
// Unnecessarily loading NDK functions dynamically via dlopen/dlsym
void* dl_handle = dlopen("libandroid.so", RTLD_NOW);
auto* from_hardware_buffer = reinterpret_cast<AHardwareBuffer* (*)(
    JNIEnv*, jobject)>(dlsym(dl_handle, "AHardwareBuffer_fromHardwareBuffer"));
AHardwareBuffer* buffer = from_hardware_buffer(env, hardware_buffer_obj);
```

Good:

```cpp
// Call standard NDK APIs directly (or check weak symbol against nullptr if > API 29)
#include <android/hardware_buffer_jni.h>

AHardwareBuffer* buffer =
    AHardwareBuffer_fromHardwareBuffer(env, hardware_buffer_obj);
```
