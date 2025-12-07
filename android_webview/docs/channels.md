# Understanding WebView Channels

Since the introduction of WebView as an updatable APK in Android L, WebView has
had some conception of pre-release channels. The details of the implementation
of these channels has differed between different generations of WebView, and
this document explains those details.

Currently we only support Android Q and above for new WebView updates. If you
would like to learn about what we did for old OS versions, see [legacy OS
behavior](legacy-os-behavior.md).

## Trichrome (Android Q+)

For Android Q+, WebView and Chrome are separately installed APKs. However,
Google began building a separate package of WebView for each of the four Chrome
channels: Stable, Beta, Dev, and Canary. Users can find each of these four
channels of WebView on the Play Store and install them simultaneously on a
single device. Users can use the "WebView implementation" menu to choose which
installed WebView the system should use.

Trichrome WebView APKs (of all channels) have a `versionCode` value with a final
digit of 3 ([to match Trichrome Chrome's
`versionCode`](https://cs.chromium.org/chromium/src/build/util/android_chrome_version.py)).

Users may also choose to opt into the beta program for Android System WebView
(com.google.android.webview). Users in this track will get "TrichromeOpenBeta,"
which is a special build variant with the same version and behavior as
TrichromeBeta but using WebView's stable channel package name. This variant has
a [unique `versionCode` value](/build/util/android_chrome_version.py) by using
a '4' instead of a '3' as the build variant digit.

## See Also

- [Try out WebView beta](/android_webview/docs/prerelease.md)
- [Chrome Release
  Channels](https://www.chromium.org/getting-involved/dev-channel)
