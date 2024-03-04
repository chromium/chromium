# Software draw deprecated

Software draw in WebView is deprecated and only maintained for backward
compatibility.

## Reasons to avoid software draw

Software draw has many drawbacks compared to hardware-accelerated draws.

*   Software draw uses more memory.
*   Software draw is very slow.
*   Software draw has known missing features, such as `<video>` or `<webgl>`.
*   New features may not be supported.
*   Software draw is not maintained or well tested, and is often broken
    by updates. Fixes to software-draw-only bugs are considered low priority.

## How to avoid software draws

[Hardware acceleration](https://developer.android.com/guide/topics/graphics/hardware-accel)
is enabled by default for all Android versions that supports Chromium WebView.
However apps should avoid accidentally disabling hardware acceleration for
WebView.

*   Do not use `android:hardwareAccelerated=false` anywhere.
*   Do not call `setLayerType(View.LAYER_TYPE_SOFTWARE)` on WebView or any
    parent of WebView.
*   Do not call `webview.onDraw` or `webview.draw` directly.

## Snapshot use case

A common use case for software draw is to obtain an image or texture snapshot
of the WebView. The officially supported way to do this is to create a
[VirtualDisplay](https://developer.android.com/reference/android/hardware/display/VirtualDisplay)
backed by a [SurfaceTexture](https://developer.android.com/reference/android/graphics/SurfaceTexture).

VirtualDisplay can be used to create a [Presentation](https://developer.android.com/reference/android/app/Presentation)
and the WebView can be attached to the Presentation. WebView will then render
into the SurfaceTexture of the VirtualDisplay.

SurfaceTexture can be consumed as a texture in OpenGL which can then be
[readback](https://developer.android.com/reference/javax/microedition/khronos/opengles/GL10.html#glReadPixels\(int,%20int,%20int,%20int,%20int,%20int,%20java.nio.Buffer\))
into a Bitmap.

## File bugs for crashes

If you are disabling hardware acceleration to avoid crashes, then please file
a bug at
https://issues.chromium.org/issues/new?component=1456456&template=1923373.
Please include the apk of the app, exact steps to reproduce the crash, and the
build fingerprint of the device. The fingerprint can be obtained with
`adb shell getprop ro.build.fingerprint`.
