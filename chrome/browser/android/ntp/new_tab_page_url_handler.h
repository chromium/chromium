// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_NEW_TAB_PAGE_URL_HANDLER_H_
#define CHROME_BROWSER_ANDROID_NTP_NEW_TAB_PAGE_URL_HANDLER_H_

class GURL;

namespace content {
class BrowserContext;
}

namespace chrome {
namespace android {

// Rewrites Android native page URLs for NTP, History, and Bookmarks:
//  - `chrome://newtab` -> `chrome-native://newtab` (or WebUI NTP on desktop
//    Android when enabled)
//  - `chrome-native://history` -> `chrome://history` (on desktop Android when
//    enabled)
//  - Translates bidirectionally between `chrome-native://bookmarks/` (including
//    `/folder/<id>` and `#<id>`) and `chrome://bookmarks/` (including `?id=`
//    and `?q=`) based on `kUseWebUiBookmarksAndroid`.
bool HandleAndroidNativePageURL(GURL* url,
                                content::BrowserContext* browser_context);

// Reverse handler for `HandleAndroidNativePageURL()` that allows the displayed
// virtual URL to update when rewriting between `chrome-native://` and
// `chrome://` URLs for Bookmarks and History, while preserving
// `chrome://newtab` virtual URLs.
bool HandleAndroidNativePageURLReverse(
    GURL* url,
    content::BrowserContext* browser_context);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_NTP_NEW_TAB_PAGE_URL_HANDLER_H_
