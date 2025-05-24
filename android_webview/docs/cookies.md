# Cookies

## Summary

WebView has some gotchas around cookies when compared to the rest of Chromium.
This doc runs through why, and where this happens.

## Cookie Manager

The [CookieManager API](https://developer.android.com/reference/android/webkit/CookieManager) needs to be useable before
the rest of WebView is initialized. To support this, we do some internal swapping around cookie stores
that you can read more about [here](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/browser/cookie_manager.h;l=46;drc=cbebd148e6898552bdd91af1c63c92b2de40beed).

## Cookie settings

Most of Chromium relies on HostContentSettingsMap to manage cookie settings/permissions.
WebView does not - delegating permissions to the app developer.

WebView also allows Android app developers to configure third party cookies per WebView which conceptually on the
desktop would be like configuring this per tab - this concept does not exist in Chromium.

In order to apply its own cookie settings, WebView essentially reports its various cookie settings _at the time of a
cookie request_. This means that the rest of Chromium does not need to know about how WebView configures cookies.
It does however mean that we often don't get changes to cookies for free. For example, the [storage access API](
https://developer.mozilla.org/en-US/docs/Web/API/Storage_Access_API) impacts whether or not a page may have 3PCs. We
need to specifically look for this in WebView and tell the rest of Chromium if we should have 3PCs or not.

The cookie settings need to be proxied for both javascript cookies via the [restricted_cookie_manager](services/network/restricted_cookie_manager.h)
and network cookies via [aw_proxying_url_loader_factory](android_webview/browser/network_service/aw_proxying_url_loader_factory.h).
For this reason, it is advised to have both javascript, and network cookie tests.
