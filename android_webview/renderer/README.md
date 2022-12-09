# //android\_webview/renderer/

This folder holds WebView's renderer-specific code.

## Folder dependencies

Like with other content embedders, `//android_webview/renderer/` can depend on
`//android_webview/common/` but not `//android_webview/browser/`. It can also
depend on content layer (and lower layers) as other embedders would (ex. can
depend on `//content/public/renderer/`, `//content/public/common/`).

## In-process renderer

On Lollipop (API 21) through Nougat MR1 (API 25) WebView has only a single
renderer, which runs in the browser process (so there's no sandboxing). The
renderer runs on a separate thread, which we would call the "renderer thread."

*** promo
Android Nougat has a [developer option][1] to enable an out-of-process renderer,
but the renderer is in-process by default.
***

## Out-of-process renderer

Starting in Oreo (API 26) WebView has a single out-of-process renderer (we
sometimes refer to this as "multiprocess mode"). This is enabled for all 64-bit
devices, for 32-bit devices with high memory, and for all devices starting in
Android 11 (API 31). Low memory 32-bit devices running API26-30 still use an
in-process renderer as before.

The out-of-process renderer is enabled by a new Android API
(`android:externalService`), to create sandboxed processes which run in the
_embedding app's context_ rather than the WebView provider's context.

Without this API, we could only declare a **fixed** number of renderer processes
to run in the WebView provider's context, and WebView (running in the app's
process) would have to pick one of these declared services to use as the
renderer process. This would be a security problem because:

* There's no trivial way for WebView (running in the app) to figure out which
  services are in-use, and reusing a service which is already in-use would mix
  content from two different apps in the same process (which violates Android's
  trust model).
* Even if we had a way to pick a not-in-use service, because WebView runs in the
  app's process, a malicious app could override this logic to intentionally pick
  an in-use service, with the goal of compromising another app on the system.
* We have to declare a fixed number of services in the manifest. Even if we
  could securely put each app's content in a separate renderer process,
  supposing we've declared N services, the N+1th app will not have an empty
  service available and will have to share.

Running renderers in the app's context ensures content from two apps are always
isolated, aligning with the Android security model.

### Recovering from renderer crashes

Starting with Oreo, Android apps have the opportunity to recover from renderer
crashes by overriding [`WebViewClient#onRenderProcessGone()`][2]. However, for
backwards compatibility, WebView crashes the browser process if the app has not
overridden this callback. Therefore, unlike in Chrome, renderer crashes are
often non-recoverable.

### Toggling multiprocess for debugging

On Android Oreo and above, you can toggle WebView multiprocess mode via adb:

```shell
# To disable:
$ adb shell cmd webviewupdate disable-multiprocess

# To re-enable:
$ adb shell cmd webviewupdate enable-multiprocess
```

Then you can check the multiprocess state by running:

```shell
$ adb shell dumpsys webviewupdate | grep 'Multiprocess'
  Multiprocess enabled: false
```

*** note
**Warning:** this setting is persistent! Remember to re-enable multiprocess mode
after you're done testing.

Changing this setting will immediately kill all WebView-based apps running on
the device (similar to what happens when you install a WebView update or change
the system's WebView provider).
***

## Multiple renderers

WebView does not support multiple renderer processes, but this may be supported
in the future.

[1]: https://developer.android.com/studio/debug/dev-options
[2]: https://developer.android.com/reference/android/webkit/WebViewClient.html#onRenderProcessGone(android.webkit.WebView,%20android.webkit.RenderProcessGoneDetail)
