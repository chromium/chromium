# Try out WebView Beta, Dev, or Canary

Using a pre-release channel of WebView allows you to test new, upcoming features
and enhancements to WebView. This is especially useful for Android app
developers who use WebView in their apps.

[TOC]

## Which channel do I want?

Like [Chrome](https://www.chromium.org/getting-involved/dev-channel), WebView
has four release channels:

- Stable channel
  - Installed and updated by default on every Android device with WebView
  - Fully tested; least likely to crash or have other major bugs
  - Updated every 2-3 weeks with minor releases, and every 6 weeks with major
    releases
- Beta channel
  - Available on Android 5 (Lollipop) and later
  - Tested before release, but not as extensively as stable
  - One major update ahead of stable, minor updates every week
- Dev channel
  - Publicly available on Android 7 (Nougat) and later
  - Two major updates ahead of stable, representing what is actively being
    developed
  - Updated once per week
  - Minimally tested
- Canary build
  - Available on Android 7 (Nougat) and later
  - Released daily
  - Includes the latest code changes from the previous day
  - Has not been tested or used

If you're looking for a specific of version of chromium, the latest versions
released to each channel can be found on [Chromium
Dash](https://chromiumdash.appspot.com/releases?platform=Android). WebView and
Chrome for Android always release together on all OS levels.

On Android 7 (Nougat) and later, you can install multiple channels at the same
time. This allows you to play with our latest code, while still keeping a tested
version of WebView around.

## How do I try a pre-release channel?

Steps depend on your version of Android:

### Android 7 through 9 (Nougat/Oreo/Pie)

Pre-release channels must be downloaded and installed as separate apps, but one
must be chosen to provide the system's WebView implementation at any given time.

1. Download a pre-release channel of Chrome from the play store, available here:
   - [Chrome Beta](https://play.google.com/store/apps/details?id=com.chrome.beta)
   - [Chrome Dev](https://play.google.com/store/apps/details?id=com.chrome.dev)
   - [Chrome Canary](https://play.google.com/store/apps/details?id=com.chrome.canary)
2. Follow the [steps to enable Android's developer options
   menu](https://developer.android.com/studio/debug/dev-options)
3. Choose Developer Options > WebView implementation (see figure)

   ![The "WebView implementation" menu](/android_webview/docs/images/webview_implementations_menu.png)

4. Choose the Chrome channel that you would like to use for WebView

#### Returning to stable WebView

1. To return to WebView stable, select Chrome again in the WebView
   implementation menu

### Android 5 or 6 (Lollipop/Marshmallow) and Android TV

Only one installation of WebView is allowed, but users can opt to receive the
latest beta updates from the Play Store.

1. [Join the beta tester program on Google
   Play](https://play.google.com/apps/testing/com.google.android.webview)
2. On your device, update Android System Webview [in the Play
   Store](https://play.google.com/store/apps/details?id=com.google.android.webview)
3. When the Play Store finishes updating, you will be using WebView beta!

#### Returning to stable WebView

1. [Leave the tester
   program](https://play.google.com/apps/testing/com.google.android.webview)
2. Uninstall all updates by visiting Settings > Apps > Android System WebView >
   Three dots menu in the top right > Uninstall updates
3. [Visit the Play Store
   page](https://play.google.com/store/apps/details?id=com.google.android.webview)
   one more time to install the latest updates to WebView stable, which will
   include important security fixes.

### Android 4.4 (KitKat) or earlier

WebView does not receive updates on these versions of Android, so the
pre-release channels of WebView are not available.

## Reporting problems with pre-release WebView

Any WebView-related bugs can be filed
[here](https://bugs.chromium.org/p/chromium/issues/entry?template=Webview+Bugs).

To best enable us to resolve the issue, please ensure you provide all of the
information requested in the bug report template.

## Command line tools

Choosing your WebView implementation on Android 7 (Nougat) or later can also
be done using adb, instead of the Settings UI:

```shell
adb shell cmd webviewupdate set-webview-implementation <packagename>
```

Package names are as follows:

|App name                |Package name                            |
|-----------------------:|----------------------------------------|
|Chrome (stable)         |com.android.chrome                      |
|Chrome Beta             |com.chrome.beta                         |
|Chrome Dev              |com.chrome.dev                          |
|Chrome Canary           |com.chrome.canary                       |

## See also
- [WebView channels in detail](/android_webview/docs/channels.md)
- [Chrome Release
  Channels](https://www.chromium.org/getting-involved/dev-channel)
- [WebView Release History](https://chromiumdash.appspot.com/releases?platform=Android)
