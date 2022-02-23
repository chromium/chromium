# Understanding WebView Channels

Since the introduction of WebView as an updatable APK in Android L, WebView has
had some conception of pre-release channels. The details of the implementation
of these channels has differed between different generations of WebView, and
this document explains those details.

[TOC]

## Standalone WebView (Android L - M)

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

### Changes in version 75.0.3770.40

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
strategy](#trichrome-android-q), Standalone WebView APKs are given a
`maxSdkVersion` value of 28, so that users who have opted into the beta program
described above do not experience the Play Store updating their Trichrome
WebView stable APK to Standalone WebView beta (since they share the same package
name).

## Monochrome (Android N - P)

For Android versions N - P, WebView is built as a part of the Chrome APK. Since
Chrome already supported side-by-side installs of different Chrome channels on
Android, a menu was added to Android's Developer Options Settings that [allows a
user to choose](/android_webview/docs/prerelease.md#android-n_o_or-p) which of
the installed versions of Chrome to use as the WebView.

## Trichrome (Android Q+)

For Android Q+, WebView and Chrome are again separately installed APKs. However,
Google began building a separate package of WebView for each of the four Chrome
channels: Stable, Beta, Dev, and Canary. Like with Monochrome, users can find
each of these four channels of WebView on the Play Store and install them
simultaneously on a single device. Also like Monochrome, users can use the
"WebView implementation" menu to choose which installed WebView the system should
use.

Trichrome WebView APKs (of all channels) have a `versionCode` value with a final
digit of 3 ([to match Trichrome Chrome's
`versionCode`](https://cs.chromium.org/chromium/src/build/util/android_chrome_version.py)).

Users may choose to opt into the beta program for Android System WebView
(com.google.android.webview), but it will have no effect on Android Q, as the
only APK of this package that gets published to the beta track is Standalone
WebView, which has a `maxSdkVersion` value of 28. While confusing, this helps
avoid the user's stable WebView installation being updated to beta, since the
preferred method for using WebView beta on Android Q is installing the separate
package from the Play Store. Without this, the same package would receive
updates from the builds of both Trichrome WebView stable (at the time of a
stable release) and Standalone WebView beta (at the time of a beta promotion).
Switching back and forth between these generations would involve very large
update deltas.

## See Also

- [Try out WebView beta](/android_webview/docs/prerelease.md)
- [Chrome Release
  Channels](https://www.chromium.org/getting-involved/dev-channel)
