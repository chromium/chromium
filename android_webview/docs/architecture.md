# WebView Architecture

## Layering

Android WebView is a [content embedder](/content/README.md), meaning it depends
on code in `//content/` and lower layers (ex. `//net/`, `//base/`), but does not
depend on sibling layers such as `//chrome/`. Android WebView can also depend on
[components](/components/README.md).

## Java and C++

Android WebView exposes Java APIs in the
[framework](https://developer.android.com/reference/android/webkit/package-summary)
and
[AndroidX](https://developer.android.com/reference/androidx/webkit/package-summary),
which are responsible for loading chromium code from the WebView provider
package. These APIs call into glue code
([`//android_webview/glue/`](/android_webview/glue/README.md) and
[`//android_webview/support_library/`](/android_webview/support_library/README.md)
respectively).

The glue layers convert to chromium-defined types in [the "AW"
layer](/android_webview/java/README.md). The AW Java types typically call into
[browser C++ code][browser] via Java Native Interface (JNI) or call into Java
methods in other layers which eventually use JNI (ex. `//content/public/`).
These AW types are the layer we write [automated instrumentation
tests](contributing-tests.md) against.

In addition to browser C++ code, WebView also has a small amount of code in
[`//android_webview/renderer/`][renderer] (renderer process code) and
[`//android_webview/common/`][common] (shared between multiple processes), which
are patterned off `//content/browser/`, `//content/renderer/`, and
`//content/common/`. The bulk of WebView's code is defined in `//content/` layer
and below.

## Processes

When an Android app embeds WebView, WebView's browser code runs in the app's
process (we call this the "browser process"). This means WebView code shares the
same address space, and we generally consider the app to be trusted just like
any other browser process code. WebView's browser process code runs in the same
**context** as the embedding application, which means it has all the same
permissions and limitations of the embedding app (ex. WebView only has network
access if the app requeested it). One consequence of this is WebView uses the
app's data directory, so each app has a separate cookie jar, network cache, etc.

WebView follows Chrome's architecture by separating browser and renderer code.
Due to platform limitations, WebView runs renderer content "in process" (in the
browser process) on Android L, M, and N devices and "out of process" (in a
separate renderer process) on Android O and above. See [this document][renderer]
for details. WebView's renderer process also runs in the app's context, although
this process is sandboxed so it actually has even fewer permissions.

WebView runs other services (ex. GPU service, Network Service) in-process on all
OS versions. This saves memory (which is why Chrome for Android does the same
thing on low-memory devices), although WebView is technically blocked because
there's [no Android API to run a non-sandboxed process under another app's
context](https://bugs.chromium.org/p/chromium/issues/detail?id=882650#c7).

Although WebView is typically embedded in other apps, it runs some code as its
own context. This includes a limited amount of UI code as well as a service. See
[`//android_webview/nonembedded/`](/android_webview/nonembedded/README.md) for
details.

## Packaging variants

Since Android Lollipop, WebView has been implemented by an updatable package. We
ship WebView to users in one of 3 packaging variants: standalone WebView,
Monochrome, and Trichrome. See [Packaging
Variants](webview-packaging-variants.md) for details.

## See also

* Check out [Android WebView 101 (2019)](https://youtu.be/qMvbtcbEkDU) ([public
  slide
  deck](https://docs.google.com/presentation/d/1Nv0fsiU0xtPQPyAWb0FRsjzr9h2nh339-pq7ssWoNQg/edit?usp=sharing))
  for more architecture details, and an overview of use cases
* [Reach out to the
  team](https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev)
  if you have more questions

[browser]: /android_webview/browser/README.md
[renderer]: /android_webview/renderer/README.md
[common]: /android_webview/common/README.md
