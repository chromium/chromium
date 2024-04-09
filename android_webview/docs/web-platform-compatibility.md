# Web platform compatibility in Android WebView

The Android WebView sometimes needs special consideration and care when making
changes to web platform APIs and behaviours. Chromium developers who make these
changes (especially deliberate interventions) may want to consult this
documentation and/or reach out to the WebView team if they're unsure how a
change might affect WebView.

[TOC]

## Why is WebView special when it comes to compatibility?

### Android has a different approach to compatibility than the web: targetSdkVersion

WebView is part of the Android platform and very widely used in the Android app
ecosystem. Android tries hard to avoid making backward-incompatible changes to
the platform; existing apps (including those the user may have paid for) are
expected to continue to work after an OS update.

One key way this is achieved is that Android apps declare a
[target SDK version](https://developer.android.com/guide/topics/manifest/uses-sdk-element)
in their manifest; this specifies the latest version of the Android SDK that the
developer considered and tested against. Behaviour changes and new restrictions
in the platform are often only applied to apps which target at least the version
in which those changes were introduced; this allows already-published apps to
remain unaffected by the changes. Android's
[release notes](https://developer.android.com/about/versions/10/behavior-changes-10)
for developers call out which changes developers can expect to see when they
update their app's target SDK to the latest version, to make it easier to test
and update apps.

To avoid apps continuing to target old SDK versions forever (e.g. to permanently
avoid newly introduced restrictions), the Play Store
[enforces a minimum](https://developer.android.com/distribute/best-practices/develop/target-sdk)
when developers upload app binaries to the store: new and updated apps must
target a sufficiently recent version. However, there is no enforcement applied
to existing app binaries: apps that are updated very infrequently or which are
no longer being actively developed at all remain on the store, and may still
target much older SDK versions. This means that the number of apps requiring any
particular piece of backward-compatibility code drops over time, but never to
zero, and in most cases Android retains the backward-compatible code path
indefinitely.

This means that even though WebView has been receiving frequent updates since
Android 5.0, app developers often still expect that incompatible changes to its
behaviour only happen in new Android releases.

### Android's Compatibility Test Suite tests some WebView behaviours

Android relies on the
[**Compatibility Test Suite** (**CTS**)](https://source.android.com/compatibility/cts)
to ensure that different devices' implementations of Android are compatible with
each other. CTS is versioned alongside the Android OS; any Android 10 device is
expected to pass CTS version 10, and so on. CTS does have minor version updates
to address bugs in the tests themselves, such as flakiness or assumptions that
are discovered not to be valid on all devices; see
[WebView's CTS docs](../tools/cts_config/README.md#changing-cts-tests-retroactively)
for more information on when this may be appropriate.

CTS contains a number of tests which test the WebView's APIs and behaviours, and
it's expected that these tests will pass even if the device has been updated to
the latest version of WebView. This presents a significant problem for web
platform changes if they cause an existing CTS test to fail; this means WebView
is no longer compatible with that Android version's expectations, and the entire
point of those expectations is that apps may also rely on them. CTS test
failures introduced by Chromium changes are generally considered P1 and
`ReleaseBlock-Dev`.

### WebView is often used to show "first-party" content

While many apps do use the WebView to display general web content that's also
intended to be used in browsers, it's also very common for apps to display what
the WebView team calls **first-party** (**1P**) content: HTML/CSS/JS which is
developed specifically for a particular app's WebView, rather than for the open
web.

The app developer generally considers their 1P content to be part of the app
itself, whether it's fetched over the network via HTTP(S) at runtime, or loaded
in an app-specific way. This content can present additional challenges for
compatibility for several reasons:

*   Developers may expect Android's general compatibility rules to apply to all
    the web platform APIs that their content interacts with, even though this is
    not the web's usual model.

*   Developers may expect their content to be considered just as trustworthy as
    the rest of their app, so may object to user agent interventions being
    applied to it; for example, they might expect it to be able to function as
    normal even if the WebView is in the background or offscreen.

*   1P content is often packaged inside the app binary, dynamically generated,
    or downloaded from servers that aren't linked to on the open web.
    *   This often makes it "invisible" to search engines, including the HTTP
        Archive data used by Google as one way to evaluate the compatibility
        impact of web platform changes, and to Chrome's UMA metrics. Using
        WebView's own UMA metrics can mitigate this.
    *   This also often makes it slower and more difficult for the developer to
        fix compatibility issues, since a new version of the app binary may have
        to be published to the Play Store, and users will not see the changes
        until they install the app update.

This means that WebView's API surface effectively includes not just the Java
APIs that apps call to control it, but also the entire web platform, and
therefore changes to Chromium need not touch any code in `//android_webview` to
be a risk.

### WebView's metrics and experiments are less mature and comprehensive than Chrome's

WebView does have crash reporting, UMA, and Finch, but in many cases the data is
noisier, less actionable, or less comprehensive for a number of reasons. This
can make it harder to rely on data to assess potential compatibility impact.

WebView's beta/dev/canary populations are fairly small and as in Chrome these
users are a self-selected sample who aren't always representative of the stable
population. A particular concern for WebView here is that even a fairly popular
Android app's user base might include significantly fewer pre-stable WebView
users than the overall population size would suggest; an issue that only affects
a few specific apps may never be encountered in the field until the stable
release.

UMA metrics implemented in `//content` (and in those `//components` which are
shared between WebView and Chrome) are generally already being recorded in
WebView. UMA metrics implemented in `//chrome` code will not be recorded in
WebView, and should be moved to lower layers of the code if they're relevant to
WebView so that they can be shared and compared.

## What are WebView's compatibility goals?

WebView has to strike a balance between allowing the web platform to move
forward, and ensuring that existing Android apps continue to work correctly.
Since it's WebView's explicit goal (and on most OS versions, a strict technical
requirement) to ship new versions alongside Chrome for Android, it's neither
realistic nor desirable to hold back the vast majority of web platform changes,
even if they present _some_ compatibility risk. WebView focuses on:

*   **Issues that affect a library, framework, or SDK that is used by more than
    one app developer.** For example: mobile ad SDKs, cross-platform app
    deployment environments like Cordova, or the AndroidX libraries.
    *   It can take a long time for apps to update to a new library version with
        the fix, because libraries are almost always compiled into the
        individual app binaries; Android apps generally can't share library code
        on-device.

*   **Issues where no straightforward fix or workaround can be applied by the
    app developer.** For example: feature removals, new restrictions, or user
    agent interventions. If the developer can't realistically update their app
    to a working state in a short period of time, there's a stronger need for
    WebView to revert the change or work around the issue on their behalf.

*   **Issues that affect common patterns of WebView API usage.** The design
    choices of WebView's Java APIs often mean that many different apps have
    similar patterns of usage, either learned from examples on the internet or
    through natural convergence of implementation; some of these patterns result
    in `//content` APIs being invoked in ways that Chrome and other browsers do
    not, and many of them rely on _de facto_ existing behaviour of WebView
    rather than documented API behaviours. Some are well-known by the WebView
    team, but others are only discovered after a change is made.

*   **Issues that affect very popular apps.** The number of installs of an app
    roughly has a long-tail distribution, so a very large number of users can be
    impacted even if just one app is affected by a change.

## What kind of changes are likely to have an effect on WebView compatibility?

### Unusual WebView behaviours

WebView has a number of behaviours and APIs that aren't shared with Chrome or
other browsers; these can cause unexpected compatibility issues with Chromium
changes, even if the change is not intended to change web-exposed behaviour.
Some of the most important examples:

#### URL and origin handling

*   WebView allows apps to use any string as a URL scheme, without requiring any
    definition of which custom schemes are valid.
    *   Requests to URLs with custom schemes trigger normal requests which the
        app can intercept to provide a response.
    *   The origin of a URL with a custom scheme is just the scheme prefix (e.g.
        `foo://`), so any URLs with the same custom scheme are considered
        same-origin.
    *   CORS is expected to work with custom schemes in the same way as with
        HTTP(S).

*   WebView allows apps to load arbitrary content as if it came from an
    arbitrary URL via
    [`loadDataWithBaseURL`](https://developer.android.com/reference/android/webkit/WebView#loadDataWithBaseURL(java.lang.String,%20java.lang.String,%20java.lang.String,%20java.lang.String,%20java.lang.String)).
    *   The navigation is treated as if it were loading the "base URL", but
        instead of making a request to that URL, the custom content is loaded as
        if it were a `data:` URI.
    *   The origin of the loaded page is the origin of the base URL.

*   WebView apps frequently use `file://` URLs to load local content; special
    settings are supported.
    *   The WebView
        [`setAllowFileAccessFromFileURLs`](https://developer.android.com/reference/android/webkit/WebSettings#setAllowFileAccessFromFileURLs(boolean))
        API causes all `file://` URLs to be considered same-origin with each
        other, instead of the default Chrome behaviour of `file://` URLs having
        an opaque origin.
    *   The WebView
        [`setAllowUniversalAccessFromFileURLs`](https://developer.android.com/reference/android/webkit/WebSettings#setAllowUniversalAccessFromFileURLs(boolean))
        API allows all `file://` URLs to make requests to **any** origin without
        CORS or any other restrictions.
    *   WebView also supports `content://` URLs to access Android content
        providers, and these are treated like `file://` URLs in most respects.

#### Process model

*   [WebView runs as a library inside the embedding application.](./architecture.md#processes)
    WebView's browser process is the app's process, and thus any process-global
    state is potentially shared with the embedding app.

*   Depending on the OS version and device configuration, WebView may run in
    [either "single-process" or "multi-process" mode.](../renderer/README.md)
    Only one renderer per app is currently used in multi-process mode, but this
    limit may increase in the future.

*   WebView cannot use non-sandboxed child processes regardless of whether it's
    running in single- or multi- process mode; only sandboxed child processes
    are supported.

*   WebView does not use a separate GPU process even when running in
    multi-process mode.

#### UI and browser chrome

*   WebView doesn't render any UI elements outside of the content viewport. The
    embedding app is responsible for any UI that may be needed to implement
    WebView callbacks.

*   Callback APIs exist to trigger permission prompts (though apps may not
    implement them), but there's no current API to render infobars, control how
    the URL or security status is shown (if at all), or similar features.

*   Cooperation from the embedding app is needed to implement features such as
    popup windows and fullscreen, and apps may not implement these, or implement
    them incorrectly.

#### Graphics

*   WebView's graphics rendering architecture is somewhat different to Chrome's
    to support specific behaviours apps rely on; see
    [the architecture doc](https://docs.google.com/document/d/1MLPEmMugdVvfeMeQQN_NMolqs4zZekfKjZeNAQJJnMo)
    for an overview.

### Issues with existing WebView APIs

#### Permissions

WebView's APIs for allowing the app to control web permissions have significant
limitations at present. The Web Permissions API is not implemented in WebView,
and in general any code in Chromium that relies on being able to silently check
the status of permissions will not work - only actual permission requests are
supported. This can cause problems when implementing web platform features that
are gated behind permissions, or when changing the way that existing features
make permissions checks.

## How do I determine the effect of a change on WebView compatibility?

### CTS

Ensuring that WebView's CTS tests all pass with your change enabled is a very
important first step; if it causes CTS failures then this means WebView
compatibility is _definitely_ affected, and means you should definitely
[reach out to the WebView team](https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev)
to discuss it.

CTS is run as part of the `android-pie-arm64-rel` trybot, and on the main
waterfall. If your change causes CTS failures, you may need to
[run CTS locally](./test-instructions.md#cts) to investigate.

### UMA, Finch, and other data collection from the field

WebView has UMA, Finch, and crash reporting, but does not currently have any
equivalent of UKM. Googlers can read the WebView-specific
[UMA](http://go/clank-webview-uma) and [Finch](http://go/clank-webview/finch)
docs for more detailed information, but some key points:

*   WebView is a separate "platform" for UMA/Finch purposes; experiments
    targeting "android" only affect Chrome for Android, _not_ WebView.

*   WebView's beta population is fairly small, and its dev and canary
    populations are tiny. The dev and canary channels are also only available on
    Android 7 and later, so data from Android 5 and 6 is limited to beta and
    stable.

*   WebView currently uses _per-app_ anonymous IDs for privacy reasons - this
    means that each individual app on a particular Android device will be
    considered a "user" or "client" for data collection purposes, with no way
    to aggregate data at the level of actual users or devices.

*   Individual WebView apps can disable metrics collection, even if the user
    opted in on their device.

*   WebView does not always record the package name of the app for privacy
    reasons.

## My change is likely to (or already did) affect WebView compatibility; what should I do?

There is no single solution here, but there are several common approaches; the
person(s) making the change should discuss it with the WebView team to decide
what's appropriate in a particular case. These are not mutually exclusive; it
may be useful to combine approaches or to move to another approach at a later
time.

### Apply the change to all apps, but use a Finch experiment for rollout

This is generally the best option if there is some reason to believe that the
change might be a compatibility issue, but there aren't any specific apps which
are known to be impacted, and there's no practical way to collect metrics on the
impact from the field.

You should implement your change behind a flag (if it isn't already), and make
sure that the flag is **disabled** by default for WebView. You should also
[expose that flag in WebView's developer UI](./developer-ui.md#Adding-your-flags-and-features-to-the-UI)
so that testers, app developers, and users can check if their issue is caused by
your change. On production devices, only the flags in the developer UI can be
used; it's not possible to pass arbitrary command line flags.

*** aside
We don't recommend enabling flags by default and relying on "kill switches" in
Finch for potentially incompatible WebView changes, because WebView apps can't
apply experiment settings from the server the first time they are launched. This
means that if an app is broken by your change, it will disproportionately affect
new users of the app, which is an understandably major concern for app
developers!
***

Before enabling the Finch experiment, reach out to the WebView team to discuss
testing; WebView's QA testers can test popular applications with your flag
enabled to give you early feedback on potential issues. If there are no issues
found at this stage, you can proceed with the Finch experiment. Googlers can
also read
[the WebView-specific Finch documentation](http://go/clank-webview/finch).

*** note
If the change causes a WebView CTS test to fail, this is usually not going to be
an option: existing versions of CTS are expected to pass even with an updated
version of WebView. Instead, you should work with the WebView team to update the
_next_ version of CTS, and only apply your change to apps which target the next
version of Android or later. An exception _may_ be made if the change fixes an
existing security issue; please discuss this with the WebView team if you think
it may apply.
***

### Apply the change to all apps unconditionally

Sometimes it may not be necessary (or reasonable) to use a Finch experiment to
roll out a change. Some examples of when this may be appropriate:

*   if the effect of the change will be immediately obvious to app developers
    who encounter it, and the appropriate fix is straightforward; e.g. if APIs
    will now return errors or throw exceptions in cases where they did not
    before, and the errors provide enough information to address the issue

*   if metrics show that the number of affected apps is very small and the issue
    is rarely encountered

*   if the change addresses a security issue

*   if the change cannot reasonably be implemented behind a flag for technical
    reasons

Even in this case, it can still be useful to have a temporary flag to revert to
the old behaviour, so that testers, app developers, and users can check if their
issue is caused by the change.

### Reach out to affected apps to ask them to fix the incompatibility

If you are able to determine that only a small number of apps are affected by
the change, it may be possible to reach out to the developers of those apps and
suggest changes that will avoid the compatibility issue; you can discuss this
with the WebView team and we can help. This is more likely to succeed if the
apps in question are popular and are actively being updated, such that a
relatively small amount of effort can resolve the issue for a large number of
users.

You should implement your change behind a flag and
[expose that flag in WebView's developer UI](./developer-ui.md#Adding-your-flags-and-features-to-the-UI)
to enable the app developer to test their app with the new behaviour.

You should also ensure that metrics are implemented which will clearly show
whether apps are still relying on the old behaviour. The metrics you already
have for the feature may be sufficient, but it may also be useful to add new
WebView-specific metrics (e.g. to see how relevant WebView Java APIs are being
used).

Once the flag and metrics are implemented and released (ideally to at least the
beta channel), the WebView team can help you reach out to the developers in
question. If the metrics show that the compatibility risk has decreased
significantly, you can consider rolling the change out via a Finch experiment as
described above.

### Apply the change to apps that declare targetSdkVersion >= X

This is often applicable when the change is adding a restriction, handling input
more strictly, or changing the interpretation of currently-accepted but dubious
or invalid inputs. If apps are using an API incorrectly or ill-advisedly, this
will allow their existing binaries to continue working, but force them to
correct their code at some point in the future when they target the new SDK
version. It also gives us a standard way to communicate the change to
developers: adding it to the release notes of the next Android version.

The downside is that the old behaviour must still be maintained, either by
keeping the old code path or by adding explicit compatibility code; both
versions need to be appropriately tested to avoid future problems, and this can
eventually become technical debt. You should expect to **maintain both versions
indefinitely**; we do not currently have a process to sunset target SDK
compatibility behaviours in WebView.

When making a change conditional on targetSdkVersion, it should only apply to
Android versions that have yet to be released to developers; once the final
Android SDK has been released (which happens while the OS update itself is still
in beta, before the general public release), it's too late to add new
targetSdkVersion-based requirements. This will often mean that you don't get any
feedback about how the new behaviour affects WebView for _at least 6-12 months_,
as apps will still be using the old behaviour for some time.

### Expose a new Java API or setting for apps to control it

If it's reasonable for an app to want to keep the old behaviour in some cases,
but the new behaviour is also useful/applicable in WebView, then it may be
desirable to give the app control. This usually either means adding a new
setting that can be configured through WebView's Java API, or adding a new Java
callback which apps can implement to make case-by-case decisions.

Where possible, settings should be configurable on a per-WebView basis
(equivalent to per-tab in Chrome) - apps often use different WebViews for very
different purposes, and may contain third party libraries that use WebView in a
manner outside of the app's direct control, so global settings can be difficult
to use successfully.

WebView's Java API can be extended via the AndroidX compatibility library, which
is released more frequently than the Android OS, and allows developers to use
new WebView APIs even on older versions of Android (as long as a new enough
version of WebView itself is installed on the device).

It's usually necessary for the default to be the _old_ behaviour, and for apps
to explicitly opt in to the new behaviour. If the new behaviour will usually be
preferable and the old behaviour is only desired in exceptional cases, one
option is to make the default conditional on targetSdkVersion (as described in
the previous section) - default to the old behaviour (with an explicit opt-in to
the new behaviour) on apps which target &lt;X, and default to the new behaviour
(with an explicit opt-out to revert to the old behaviour) on apps which target
&gt;=X.

### Last resort: don't apply the change to WebView at all

This is usually only applicable when it makes sense for the change to be a
matter of user agent policy, e.g.:

*   if it's controlled by a normal user preference in Chrome (**not** by a flag
    or command line switch)

*   if it's controlled by an enterprise policy in Chrome that's not intended to
    expire in a future release

*   if it's explicitly stated to be user agent defined in web specifications

*   if other browsers behave differently and aren't going to adopt the same
    behaviour

This is generally a last resort even if these apply; every place WebView
entirely diverges from Chrome may become a source of technical debt and other
issues later. Please talk to the WebView team before deciding to omit WebView
from a web platform change!

## How do I implement behaviours/options differently in WebView than Chrome?

All code in `//android_webview` is exclusively used in WebView, and no code in
`//chrome` is ever used by WebView, so if you can make the change at that level,
it's guaranteed not to affect the other.

In common code (such as `//content` and `//components`), it is **not** possible
to use `#if` or similar to add/exclude code just for WebView; the distinction
must be made at runtime. There is also intentionally no generic API or command
line flag for "is this WebView" (and if you do find something that looks like
that, please don't use it!) - it should be explicit exactly what
behaviour/option is being controlled in each case.

*** aside
All common code is only compiled once for both WebView and Chrome for Android,
for unavoidable packaging reasons, so there is no `WEBVIEW` or similar macro
defined. Code can use `#if BUILDFLAG(IS_ANDROID)` to exclude it from other
platforms if desired, but this includes Chrome for Android.
***

Mechanisms usually used for this include:


*   Checking a command line flag in the common code such as
    `--disable-foo-enforcement`, and setting that flag during WebView's startup,
    often in [`AwMainDelegate::BasicStartupComplete`](../lib/aw_main_delegate.cc).
    *   This can be used to apply changes conditionally based on the app's
        targetSdkVersion, or unconditionally for all usage of WebView. Don't use
        this for settings the app should be able to control.
    *   The command line flag should have a name that describes the actual
        effect it has, and usually doesn't need to refer to WebView.

*   Calling a method defined by the common code from WebView code.
    *   This can be used during startup as a more flexible alternative to a
        command line flag; for example to easily pass an enum value, or to
        ensure the change is applied at a specific point in time.
    *   This can also be used when the app should be able to change a global
        setting at runtime.

*   Checking a setting stored in `content::WebPreferences`.
    *   This can be used when the app should be able to change the setting at
        runtime, usually on a per-WebView basis (e.g. via
        `android.webkit.WebSettings`).

*   Calling a method in a delegate interface defined by the common code and
    implemented by the WebView/Chrome layer.
    *   This can be used for situations that are more complex than a
        boolean/enum option, e.g. policy logic with parameters that affect the
        result, if the policy should be different in Chrome vs WebView.
