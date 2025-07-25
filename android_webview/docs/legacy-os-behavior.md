# Legacy OS versions

Android WebView originally supported updates on Android L+. Over the past few
years we have started dropping support for old OS versions. This document covers
topics related to those old OS versions but which are no longer relevant for
WebView development on the latest chromium versions.

[TOC]

## Build targets

WebView originally supported build targets based on device OS version:

```shell
# For L+ (21+) devices (if on N-P, see "enable redundant packages setting")
autoninja -C out/Default system_webview_apk

# For N-P (24-28) devices (not including TV/car devices)
autoninja -C out/Default monochrome_public_apk

# For Q+ (29+) devices
autoninja -C out/Default trichrome_webview_apk
```

## WebView package names

WebView's original build targets have a variety of package names:

| API level            | Has GMS vs. AOSP? | Allowed package names |
| -------------------- | ----------------- | --------------------- |
| L-M                  | AOSP    | `com.android.webview` **(default, preinstalled)** |
| L-M                  | Has GMS | `com.google.android.webview` **(default, preinstalled)** |
| N-P                  | AOSP    | `com.android.webview` **(default, preinstalled)** |
| N-P (TV/car devices) | Has GMS | `com.google.android.webview` **(default, preinstalled)** |
| N-P (other devices)  | Has GMS | `com.android.chrome` **(default, preinstalled)**<br>`com.chrome.beta`<br>`com.chrome.dev`<br>`com.chrome.canary`<br>`com.google.android.apps.chrome` **(only userdebug/eng)**<br>`com.google.android.webview` **(preinstalled)** (see [Important notes for N-P](build-instructions.md#Important-notes-for-N-P)) |
| >= Q                 | AOSP    | `com.android.webview` **(default, preinstalled)** |
| >= Q                 | Has GMS | `com.google.android.webview` **(default, preinstalled)**<br>`com.google.android.webview.beta`<br>`com.google.android.webview.dev`<br>`com.google.android.webview.canary`<br>`com.google.android.webview.debug` **(only userdebug/eng)**<br>`com.android.webview` **(only userdebug/eng)** |

`monochrome_public_apk`'s package name defaults to `org.chromium.chrome`.
Because this not accepted by any build of the OS, you'll want to change this
with the GN arg `chrome_public_manifest_package =
"com.google.android.apps.chrome"`, or choose `system_webview_apk` instead.

## Enable redundant package setting on Android N-P

_This functionality was added in N and removed in Q, so you should ignore this if
your device is L-M or >= Q._

If you have an Android build from N-P (and, it uses the Google WebView
configuration), then the `com.google.android.webview` package will be disabled
by default. More significantly, installing a locally compiled APK won't work,
since the on-device WebViewUpdateService will immediately uninstall such
updates. To unblock local development, you can re-enable the WebView package and
disable this behavior from WebViewUpdateService with:

```shell
# Only necessary if you're using the 'com.google.android.webview' package name
adb shell cmd webviewupdate enable-redundant-packages
```

## Channels
### Standalone WebView (Android L - M)

The pre-release channels strategy of Chrome for Android has been to build
separate APKs, each with a unique package name, so that they can be installed
side-by-side. In contrast, Standalone WebView has only a single package name
(com.google.android.webview) that is recognized by Android as a valid WebView
provider, and instead utilizes [the "tracks"
API](https://developers.google.com/android-publisher/tracks) provided by Google
Play. This allows users to opt-in to beta updates, but users cannot have
simultaneous installations of both stable and beta Standalone WebView on a
single device.

All users that have Android System WebView installed and enabled are eligible to
receive updates published to the production track. In addition, users can opt-in
to the beta track by [joining the beta tester
program](/android_webview/docs/prerelease.md#android-l-or-m-plus-all-versions-of-android-without-chrome).
Once a user's account is in the beta program, it is eligible to receive updates
that have been published to the beta track. The Play Store updates users to the
APK with the highest `versionCode` among eligible tracks.

Google publishes an APK of Android System WebView to the beta track based on the
[same release cycle](https://chromiumdash.appspot.com/schedule) as Chrome Beta.

Similarly, builds of Chrome for "Dev" channel releases include a build of
WebView which gets published to an alpha track. Only users belonging to specific
internal Google groups are eligible for Play Store updates from Android System
WebView alpha track, including members of Google's internal Chrome for Android
and Android WebView teams.

#### Changes in version 75.0.3770.40

Prior to version 75, WebView's internal logic had no way of knowing whether it
had been built for a dev, beta, or stable release. Beginning with version
75.0.3770.40, Google builds separate APKs for each of stable, beta, and dev
channels, with the internal logic differing only in that it is aware of which
channel it was built for.

This change allows for different logic flow depending on the channel, such as
sampling crash reports differently, targeting beta users for experiments, etc.

With this change, the WebView APKs belonging to the three different channels
were given [unique `versionCode` values](/build/util/android_chrome_version.py),
with stable having a final digit of 0, beta a final digit of 1, and dev 2.
Having identical package names required unique versionCodes for the different
channel builds to exist simultaneously on platforms like Google Play and
Google's internal crash and performance analytics infrastructure. These specific
values were chosen so that a user who has opted into the beta program will still
receive the beta APK (Play Store delivers the APK with the higher `versionCode`
value) even if a stable APK from the same build is available (such as when we
promote a beta build to stable), so that the user can receive beta-targeted
experiments.

To avoid conflicting with [Trichrome WebView's pre-release channel
strategy](channels.md#trichrome-android-q), Standalone WebView APKs are given a
`maxSdkVersion` value of 28, so that users who have opted into the beta program
described above do not experience the Play Store updating their Trichrome
WebView stable APK to Standalone WebView beta (since they share the same package
name).

### Monochrome (Android N - P)

For Android versions N - P, WebView is built as a part of the Chrome APK. Since
Chrome already supported side-by-side installs of different Chrome channels on
Android, a menu was added to Android's Developer Options Settings that [allows a
user to choose](/android_webview/docs/prerelease.md#android-n_o_or-p) which of
the installed versions of Chrome to use as the WebView.

## WebView loading

See [how does loading work](how-does-loading-work.md) for an full explanation.

## AOSP integration

See [AOSP system integration](aosp-system-integration.md) for full instructions.
Monochrome is **no longer recommended** and you should use standalone WebView or
Trichrome instead.

### Monochrome build variant

If your AOSP device will include a default web browser based on Chromium, it may
be beneficial to use Monochrome as the WebView implementation. Monochrome is
compatible with Android 7.x (Nougat), 8.x (Oreo) and 9.x (Pie), but not with
Android Q and later due to changes made to support Trichrome.

Monochrome is a single APK which contains both the entire WebView
implementation, and also an entire Chromium-based web browser. Since WebView and
the Chromium browser share a lot of common source code, the Monochrome APK is
much smaller than having a separate WebView APK and browser APK.

However, Monochrome can make it more difficult for you to allow the user to
freely choose their own web browser and can have other downsides: see
[this section](#Special-requirements-for-Monochrome) for more information.

The build target is called `monochrome_public_apk` and the resulting output file
is called `MonochromePublic.apk`.

### Android 7.x (Nougat), 8.x (Oreo), and 9.x (Pie) configuration

This is the same configuration format as for Trichrome, however you would
specify Chrome package names rather than WebView package names. See [AOSP system
integration](aosp-system-integration.md) for details.

```shell
# For an APK or Bundle target compiled from chromium (replace
# "system_webview_apk" with your build target):
$ out/Default/bin/system_webview_apk print-certs --full-cert

# For a pre-compiled APK or Bundle:
$ build/android/apk_operations.py print-certs --full-cert \
  --apk-path /path/to/AndroidWebview.apk
```

The one major difference is the `isFallback` attribute. On Android N, O, and P
this attribute means:

```
      isFallback (default false): If true, this provider will be automatically
          disabled by the framework, preventing it from being used or updated
          by app stores, unless there is no other valid provider available.
          Only one provider can be a fallback. See "Special requirements for
          Monochrome" to understand one possible use case.
```

### Android 5.x (Lollipop) and 6.x (Marshmallow)

The name of the WebView package is specified as a string resource in the
framework. The default value is located in
`frameworks/base/core/res/res/values/config.xml` under the resource name
`config_webViewPackageName` - you can either edit this file in place, or create
a new configuration file for your product and include it as a resource overlay
using the `PRODUCT_PACKAGE_OVERLAYS` build variable.

### Special requirements for Monochrome

Standalone WebView is not generally visible to the user unless they go looking
for it, since it's a system app with no UI. In contrast, Monochrome is a web
browser as well as a WebView implementation, which makes it much more visible,
since it's a normal app with a launcher icon.

This means that the user may wish to be able to disable Monochrome in some
circumstances; for example, if they prefer to use a different web browser.
Allowing the user to do this without breaking applications which use WebView can
be difficult - on current versions of Android, WebView cannot function correctly
if the package is disabled, and app stores generally do not update disabled
apps, so the user would not receive security updates. An alternate WebView
implementation must be provided on the device to allow users to disable
Monochrome without unwanted side effects.

The simplest option is to also preinstall the standalone WebView, and mark it as
`isFallback="true"` in the framework WebView configuration. The framework will
then automatically disable the standalone WebView in most cases, and will only
re-enable it if the user does in fact disable Monochrome. This will allow the
implementation that the user is using to receive updates, while blocking
unnecessary updates of the implementation that is not being used.

However, preinstalling both Monochrome and the standalone WebView takes up a lot
of space, as they're both quite large. To avoid this, it's possible to
preinstall a "stub" version of the standalone WebView instead, which is much
smaller, but still performs the function of a full WebView APK by using a
special codepath in the framework: it loads its actual implementation from the
preinstalled Monochrome APK at runtime. The stub and Monochrome must be matching
versions for this to work - they should be built together in a single build,
and any time you build a new version of Monochrome you should also rebuild the
stub.

Currently, the public Chromium source code does **not** contain a suitable stub
WebView implementation. Please [contact the WebView team][1] for assistance if
you're planning to ship a configuration based on Monochrome.

## Native library

On Android L, the native library must be called
`libwebviewchromium.so`. Starting with Android M and above, the native library must be
declared by the `com.android.webview.WebViewLibrary` metadata tag in
`AndroidManifest.xml`. See [Loading native code with RELRO
sharing](how-does-loading-work.md#Loading-native-code-with-RELRO-sharing) for
more details if you're curious how this process works.

[1]: https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev
