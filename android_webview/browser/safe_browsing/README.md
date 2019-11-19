# Android WebView Safe Browsing

[TOC]

Android WebView has supported core Safe Browsing features since 2017.

*** promo
Googlers may wish to consult [internal
documentation](http://go/clank-webview/safebrowsing).
***

## What is Safe Browsing?

See the relevant Chromium classes in
[//components/safe\_browsing/](/components/safe_browsing).

For info on the feature, see https://safebrowsing.google.com/.

## Opt-in/consent/requirements

### Google Play Services

If Google Play Services (AKA GMSCore) is uninstalled, disabled, or out-of-date,
WebView cannot perform Safe Browsing checks (with the exception of [hard-coded
URLs](#hard_coded-urls)). Before trying Safe Browsing locally, make sure this is
up-to-date:

```shell
$ adb shell am start -a "android.intent.action.VIEW" -d "market://details?id=com.google.android.gms"
# Then, manually update GMS in the UI.
```

If Google Play Services is installed, the user must opt into Google Play
Protect's "Verify Apps" setting: `Settings > Google > Security > Google Play
Protect > Scan device for security threats`.

### Application opt-in

Safe Browsing is enabled by default, but applications can explicitly disable it
with a manifest tag:

```xml
<manifest>
    <application>
        <meta-data android:name="android.webkit.WebView.EnableSafeBrowsing"
                   android:value="false" />
        ...
    </application>
</manifest>
```

## Hard-coded URLs

WebView supports Safe Browsing checks (for testing purposes) on hard-coded WebUI
URLs defined in
[`//components/safe_browsing/web_ui/constants.cc`](/components/safe_browsing/web_ui/constants.cc)
(ex. `chrome://safe-browsing/match?type=malware`).

These URLs don't show meaningful content, but will trigger an interstitial when
trying to navigate to them. WebView relies on these URLs in our CTS tests, so
they **must never change** (but more URLs may be added).

## Differences in support and types of interstitials

See [this page](docs/differences.md).

## Testing Safe Browsing

Automated tests live
[here](/android_webview/javatests/src/org/chromium/android_webview/test/SafeBrowsingTest.java).

You can manually test Safe Browsing with the [WebView
Shell](/android_webview/docs/webview-shell.md). Navigate to one of the
[hard-coded URLs](#hard_coded-urls) mentioned above.

To test more complex scenarios and WebView's Safe Browsing APIs, please try out
the [open source WebView demo
app](https://android.googlesource.com/platform/frameworks/support/+/HEAD/webkit/integration-tests/testapp).

*** note
**Note:** if testing Safe Browsing manually, make sure to [update GMS and
opt-into Google Play Protect](#Google-Play-Services).
***

## Supporting new threat types

As Chrome supports more threat types, so can WebView. The steps are:

1. Create quiet interstitial resources for the new threat type ([example
   CL](https://chromium-review.googlesource.com/c/chromium/src/+/1256021)).
1. Whitelist [resources](/android_webview/ui/grit_resources_whitelist.txt) and
   [strings](/android_webview/ui/grit_strings_whitelist.txt) (
   [general docs](/android_webview/ui/README.md), [example
   CL](https://chromium-review.googlesource.com/c/chromium/src/+/1270476/12/android_webview/ui/grit_strings_whitelist.txt)).
1. Add the new threat type to our list of threats ([example
   CL](https://chromium-review.googlesource.com/c/chromium/src/+/1270476/12/android_webview/browser/aw_url_checker_delegate_impl.cc)).
1. Add a hard-coded URL ([example
   CL](https://chromium-review.googlesource.com/c/chromium/src/+/1270476/12/components/safe_browsing/web_ui/constants.cc)).
1. Write integration tests ([example
   CL](https://chromium-review.googlesource.com/c/chromium/src/+/1270476/12/android_webview/javatests/src/org/chromium/android_webview/test/SafeBrowsingTest.java)).
1. Add a new threat type constant to the Android SDK (constants are defined in
   `WebViewClient.java`, please [consult a WebView team
   member](https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev)
   before this step). The new threat type constant should only be used when the
   application targets the new Android SDK: use
   [SAFE\_BROWSING\_THREAT\_UNKNOWN](https://developer.android.com/reference/android/webkit/WebViewClient.html#SAFE_BROWSING_THREAT_UNKNOWN)
   for apps with older targetSdkVersions (see http://crbug.com/887186#c15 and
   http://b/117470538).
