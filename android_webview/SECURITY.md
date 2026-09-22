# Android WebView Security Guidance for AI Agents

Android WebView (sometimes "Android System WebView" or just "WebView") is an
Android OS library that allows apps to embed web content.

Open source Android WebView code is divided across three repositories:

* Chromium - The WebView implementation that ships as an APK, and canonical
  "glue layer" definitions for APIs to hook into.
* Android framework
  ([android.webkit](https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/webkit/)) -
  Public APIs that generally do not receive new features.
* AndroidX/Jetpack
  ([androidx.webkit](https://cs.android.com/androidx/platform/frameworks/support/+/androidx-main:webkit/)) -
  Public APIs that are frequently updated and made available on a wide variety
  of Android OS versions.

The WebView team also owns some additional products that ship as part of the
WebView APK:

* JavaScriptEngine (`//android_webview/js_sandbox` in the Chromium repo), which
  allows an app to run JavaScript code in an isolated/sandboxed process running
  V8. The app library for JavaScriptEngine is
  [androidx.javascriptengine](https://cs.android.com/androidx/platform/frameworks/support/+/androidx-main:javascriptengine/javascriptengine/).
* A PAC Processor which uses V8 to run Proxy Auto-Configuration
  scripts. Entrypoints/APIs [live under the android.webkit
  package](https://cs.android.com/android/platform/superproject/+/android-latest-release:frameworks/base/core/java/android/webkit/PacProcessor.java)
  in AOSP. The PAC processor ordinarily runs directly within an [unprivileged
  app/service](https://cs.android.com/android/platform/superproject/+/android-latest-release:frameworks/base/packages/services/PacProcessor/)
  process with network access and is typically invoked via
  [PacProxySelector](https://cs.android.com/android/platform/superproject/+/android-latest-release:frameworks/base/core/java/android/net/PacProxySelector.java),
  though these callers are not owned by the WebView team.

Much of the advice from
[//docs/security/security-for-agents.md](/docs/security/security-for-agents.md)
applies also to WebView, though there are some key differences between WebView
and full Chromium-based browsers like Chrome. Details in this document take
precedence over that general document.

## Trust model

The application and any library/SDK code that runs under its main process are
considered ultimately trusted. App developers may intentionally configure or
disable security features (such as Safe Browsing), and this is not considered a
WebView bug. The OS and hardware are also ultimately trusted.

There is a small exception to this with regard to Android WebView Media
Integrity, which reaches out to OS-level integrity providers to acquire
cryptographic attestations about the device, system, and host app. This
typically provides web services with some defence against abusive or fraudulent
apps/devices. (It does not provide the device's user with any guarantees.)

In regular production scenarios where neither the app nor OS is
debuggable/rooted, the user is **not** fully trusted. This is different to most
regular browser trust models.

* Users should not be able to attack the application via WebView, for example,
  via chrome://inspect or attacks that leak or modify app directory data
  inappropriately. However, there can be cases where known-insecure APIs or
  WebSettings chosen by apps can allow this to happen, but this would be
  considered an app bug rather than a vulnerability in the WebView library (see
  [Insecure API usage](#insecure-api-usage)).
* Users should not be able to control arbitrary feature flags/params other than
  those published by the WebView DevTools UI.
* User-installed CA certificates are ignored (unless opted into by the app).

However, users may have the following capabilities:

* Users are generally able to downgrade WebView, which may re-introduce
  vulnerabilities. This is only partially mitigable by WebView itself, for
  example, by clearing browsing state upon detecting a downgrade, and
  OS-enforced version restrictions. Paranoid apps may enforce their own version
  restrictions.
* Users are in full control of debuggable or rooted devices and apps.

Regular apps should not be able to attack each other.

WebView makes use of a "non-embedded" service that is shipped with the WebView
APK and runs outside of the app. This service supplies configuration to apps and
is considered highly trusted. Conversely, the non-embedded service must not
trust data supplied by apps.

## Key differences compared to full Chromium-based browsers

WebView uses a reduced number of processes compared to Chrome in order to
conserve system resources. WebView does not have separate utility processes, and
there is generally only one renderer process per browser context (or "Profile"),
meaning there is little process-level site isolation in WebView today. However,
the WebView team is still interested in tracking bugs that would undermine
future efforts to adopt stronger site isolation in WebView (albeit with low
priority).

WebView does not use third-party storage partitioning or network state
partitioning, due to issues with API complexity, app compatibility, and disk
usage. Though third-party cookies in WebView are disabled by default for modern
apps, this means there are tracking alternatives to third-party cookies that
WebView cannot feasibly restrict.

Certain permissions are granted or denied to web content via callbacks to the
embedding application. Not all apps present a choice to the user for all types
of permissions. Due to app compatibility or limitations in API structure,
certain permissions are currently auto-granted to web content without consulting
the application.

WebView inherits certain security policies from the Android OS and the
application, particularly regarding network-security-config.xml. This
particularly affects TLS certificate verification.

## Insecure API usage

The power of the WebView APIs may allow apps to make bad decisions that
undermine their security. WebView should try to minimize this risk via
documentation, defaults, and well-lit paths. However, much of this functionality
cannot be changed or removed without breakage app compatibility. Apps generally
assume the responsibility for using APIs with documented negative security
properties. However, despite common tooling warnings, WebView's threat model
considers it entirely appropriate for most apps to enable JavaScript.

Some APIs are often misused to implement security controls under false
assumptions. These are not always directly documented:

* Many APIs provide URLs, either as return values or as callback
  parameters. These URLs often have subtly different meanings and caveats, do
  not trigger under all conditions, and often do not guarantee that the WebView
  is currently on that URL right now. There are also competing concepts as to
  what the "current URL" of a WebView means. We have generally considered
  onPageStarted to be the most reliable callback to use when a URL update is
  important, as this should represent a navigation commit.
* Relying on shouldInterceptRequest to implement security controls is
  discouraged. There are many edge cases where it either doesn't trigger for
  certain requests or doesn't provide adequate metadata.
* Untrusted URLs should not be supplied to the CookieManager, as it may attempt
  to "fix up" an address in unexpected ways that are inconsistent with other
  parts of WebView.
