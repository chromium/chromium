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
  - Available on Android 7 (Nougat) and later
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

## How do I try a pre-release channel? {#switch-channel}

Steps depend on your version of Android:

### Android 10 and later (Q, R, etc.) - Beta channel {#trichrome-beta}

We offer a new streamlined experience for joining the WebView Beta channel (if
you want to opt into Dev or Canary, or you are interested in using WebView
DevTools to try experimental WebView features, [skip to the next
section](#trichrome-dev)).

The easiest way to start testing WebView Beta is to opt into the experience on
the Google Play Store.

1. [Join the beta tester program on Google Play.][WebView tester program]
2. On your device, update **Android System WebView** [in the Play
   Store.][WebView PlayStore]
3. When the Play Store finishes updating, you will be using WebView Beta!

#### Returning to stable WebView

1. [Leave the tester program.][WebView tester program]
2. Uninstall all updates by visiting Settings > Apps > Android System WebView >
   Three dots menu in the top right > Uninstall updates.
3. [Visit the Play Store page][WebView PlayStore] one more time to install the
   latest updates to WebView stable, which will include important security
   fixes.


### Android 10 and later (Q, R, etc.) - Dev and Canary channel {#trichrome-dev}

Dev and Canary channels must be downloaded and installed as separate apps, but
only one must be chosen to provide the system's WebView implementation at any
given time.

*** note
WebView pre-release channels are independent of Chrome in Android 10 and later.
Chrome can no longer be used as a WebView implementation in Android 10 and
later.
***

1. Download a pre-release channel of WebView from the play store, available here:
   - [WebView Beta](https://play.google.com/store/apps/details?id=com.google.android.webview.beta)
     may be installed either through this method or through the [streamlined
     approach mentioned above](#trichrome-beta). Installing through this method
     will automatically install WebView DevTools.
   - [WebView Dev](https://play.google.com/store/apps/details?id=com.google.android.webview.dev)
   - [WebView Canary](https://play.google.com/store/apps/details?id=com.google.android.webview.canary)
2. Follow the [steps to enable Android's developer options
   menu](https://developer.android.com/studio/debug/dev-options)
3. Choose Developer Options > WebView implementation (see figure)

   ![The "WebView implementation" menu](/android_webview/docs/images/webview_implementations_menu_10.png)

4. Choose the channel that you would like to use for WebView
5. **Bonus:** you'll also now see an icon for your chosen WebView channel in the
   list of apps. You can use this app to report bugs, toggle experimental
   features, and much more! See the [WebView DevTools user guide] for more
   details.

#### Returning to stable WebView

1. To return to WebView stable, select "Android System WebView" again in the
   WebView implementation menu

### Android 7 through 9 (Nougat/Oreo/Pie)

Pre-release channels must be downloaded and installed as separate apps, but only
one must be chosen to provide the system's WebView implementation at any given
time.

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

### Android Auto and Android TV

Only one installation of WebView is allowed, but users can opt to receive the
latest beta updates from the Play Store.

1. [Join the beta tester program on Google Play.][WebView tester program]
2. On your device, update **Android System WebView** [in the Play
   Store.][WebView PlayStore]
3. When the Play Store finishes updating, you will be using WebView Beta!
4. **Bonus:** you'll also now see the "WebView Beta" icon in your list of apps.
   You can use this app to report bugs, toggle experimental features, and much
   more! See the [WebView DevTools user guide] for more details.

#### Returning to stable WebView

1. [Leave the tester program.][WebView tester program]
2. Uninstall all updates by visiting Settings > Apps > Android System WebView >
   Three dots menu in the top right > Uninstall updates.
3. [Visit the Play Store page][WebView PlayStore] one more time to install the
   latest updates to WebView stable, which will include important security
   fixes.

### Android 5 or 6 (Lollipop/Marshmallow)

We no longer support devices running Android 5 or 6. Devices on these old OS
versions can still update to the last supported WebView release but will not be
able to install further updates.

### Android 4.4 (KitKat) or earlier

WebView does not receive updates on these versions of Android, so the
pre-release channels of WebView are not available.

## Reporting problems with pre-release WebView

Any WebView-related bugs can be filed
[here](https://issues.chromium.org/issues/new?component=1456456&template=1923373).

To best enable us to resolve the issue, please ensure you provide all of the
information requested in the bug report template.

## Work profile, multiple users, or Samsung Secure Folder {#multiple-profiles}

*** aside
This only applies to Android 8 (Oreo) and above.
***

If your Android device has been configured with a work profile, you'll need to
install pre-release WebView for both your work profile and regular profile. You
can only change WebView channel if you've enabled this for all profiles on the
device. Then you should be able to successfully switch WebView channels
following the steps above.

Some devices [may support multiple user
accounts](https://support.google.com/nexus/answer/2865483?hl=en). Similar to
work profile, your pre-release WebView channel must be installed and enabled for
each user account on the device.

Some Samsung phones support a feature called [Secure
Folder](https://www.samsung.com/global/galaxy/what-is/secure-folder/). Under the
hood, this is implemented by creating a new user profile, similar to work
profile or multiple users. If you've previously enabled the Secure Folder
feature, you'll need to add your WebView channel to the folder. Open the folder,
click "add apps," and select the desired WebView channel. Then you should be
able to select WebView from the menu above.

## Command line tools

Choosing your WebView implementation on Android 7 (Nougat) or later can also
be done using adb, instead of the Settings UI:

```shell
adb shell cmd webviewupdate set-webview-implementation <packagename>
```

Package names are as follows:

|App name                    |Package name                            |
|---------------------------:|----------------------------------------|
|Chrome (stable, 7/8/9 only) |com.android.chrome                      |
|Chrome Beta (7/8/9 only)    |com.chrome.beta                         |
|Chrome Dev (7/8/9 only)     |com.chrome.dev                          |
|Chrome Canary (7/8/9 only)  |com.chrome.canary                       |
|WebView (stable)            |com.google.android.webview              |
|WebView Beta (10+ only)     |com.google.android.webview.beta         |
|WebView Dev (10+ only)      |com.google.android.webview.dev          |
|WebView Canary (10+ only)   |com.google.android.webview.canary       |

## See also
- [WebView channels in detail](/android_webview/docs/channels.md)
- [Chrome Release
  Channels](https://www.chromium.org/getting-involved/dev-channel)
- [WebView Release History](https://chromiumdash.appspot.com/releases?platform=Android)

[WebView DevTools user guide]: https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/docs/developer-ui.md
[WebView PlayStore]: https://play.google.com/store/apps/details?id=com.google.android.webview
[WebView tester program]: https://play.google.com/apps/testing/com.google.android.webview
