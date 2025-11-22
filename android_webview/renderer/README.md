# //android\_webview/renderer/

This folder holds WebView's renderer-specific code.

## Folder dependencies

Like with other content embedders, `//android_webview/renderer/` can depend on
`//android_webview/common/` but not `//android_webview/browser/`. It can also
depend on content layer (and lower layers) as other embedders would (ex. can
depend on `//content/public/renderer/`, `//content/public/common/`).

## In-process renderer

WebView used to run in "single process" mode, which is when the renderer code
runs inside the browser process on a separate thread called the renderer thread.
Because this runs inside the browser process, there is no sandboxing (a
compromised renderer
has permission to access the disk or do anything else which the
browser process is capable of).

*** note
**Note:** this is largely obsolete and irrelevant. The in-process renderer was
the default on Lollipop (API 21) through Nougat MR1/MR2 (API 25), however modern
WebView releases have [dropped support for these versions][1].

Devices running Oreo (Api 26) through Q (API 29) will generally use an
out-of-process renderer (see next section), however it's possible these will use
in-process renderer on low-memory devices. However memory optimizations in
Android R (API 30) mean that WebView **always** uses out-of-process renderer on
Android R and above.

As of M139, the [only supported configuration][2] using single process mode is
Android Q low-memory devices.
***

## Out-of-process renderer

Starting in Oreo (API 26) WebView has a single out-of-process renderer (we
sometimes refer to this as "multiprocess mode"). This is enabled for all 64-bit
devices, for 32-bit devices with high memory, and for all devices starting in
Android 11 (API 31). Low memory 32-bit devices running API26-30 still use an
in-process renderer as before.

Note that in this mode, the renderer process and the host app ("browser process")
may have different bitness. See [architecture.md](../docs/architecture.md).

The out-of-process renderer is enabled by new Android APIs
(`android:externalService` and [Content.bindIsolatedService][3]), to create sandboxed processes which run in the
_embedding app's context_ rather than the WebView provider's context. These
processes will be named something like
`com.google.android.webview:sandboxed_process0` and it will run an
Android service named `org.chromium.content.app.SandboxedProcessService0`. The
package name will match the current WebView provider and the number suffix will
usually be a `0` or a `1`.

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
crashes by overriding [`WebViewClient#onRenderProcessGone()`][4]. However, for
backwards compatibility, WebView crashes the browser process if the app has not
overridden this callback. Therefore, unlike in Chrome, renderer crashes are
often non-recoverable.

## Writing automated tests for either single process or multiprocess mode

You can annotate WebView javatests with `@OnlyRunIn`. See [test instructions][5]
for details about how to use this annotation.

The default behavior (if no annotation is specified) is that the test will run
in both modes.

## Multiple renderers

Apps can create multiple WebView Profiles, in which case each Profile gets its
own renderer process. Please see [WebViewCompat.setProfile][6] if you would like
to use multiple Profiles for different WebView instances.

WebView does not generally support multiple renderer processes in a single
profile, however this may be supported in the future. The only exception today
is that WebView can create a separate renderer process for showing builtin error
pages (known as `webui` in Chromium architecture), such as Safe Browsing
interstitial warnings.

## See also

Learn about [Chrome Android Sandbox Design][7] to understand how WebView's
renderer process is sandboxed to mitigate the security impact of a compromised
renderer.

[1]: https://groups.google.com/a/chromium.org/g/chromium-dev/c/B9AYI3WAvRo/m/tpWwhw4KBQAJ
[2]: https://groups.google.com/a/chromium.org/g/chromium-dev/c/vEZz0721rUY/m/pUIgqXxNBQAJ
[3]: https://developer.android.com/reference/android/content/Context#bindIsolatedService(android.content.Intent,%20int,%20java.lang.String,%20java.util.concurrent.Executor,%20android.content.ServiceConnection)
[4]: https://developer.android.com/reference/android/webkit/WebViewClient.html#onRenderProcessGone(android.webkit.WebView,%20android.webkit.RenderProcessGoneDetail)
[5]: /android_webview/docs/test-instructions.md#instrumentation-test-process-modes
[6]: https://developer.android.com/reference/androidx/webkit/WebViewCompat#setProfile(android.webkit.WebView,java.lang.String)
[7]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/android-sandbox.md
