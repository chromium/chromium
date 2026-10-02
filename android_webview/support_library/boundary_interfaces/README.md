# Boundary Interfaces for WebView's Jetpack libraries

**Note:** the canonical copy of the boundary interfaces lives in the chromium
repository; this subdirectory of chromium is mirrored into the Android project
for use by the webkit and web AndroidX modules.

If you're reading this file in the Android repository, don't make changes in
this folder. Please make changes in
[chromium](https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/support_library/boundary_interfaces),
or contact the OWNERS in `frameworks/support/webkit/` or `frameworks/support/web/`.

If you're reading this file in the chromium repository, you should feel free to
make changes. Please be aware that this folder has strict import requirements
(enforced by `DEPS`), because it must continue to build when mirrored into
Android.
