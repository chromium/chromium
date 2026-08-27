// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ANDROID_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ANDROID_BRIDGE_H_

#include <string_view>

namespace content {
class WebContents;
}

class GURL;
class TabAndroid;

namespace send_tab_to_self {

// Attaches a visual label indicating the sender device name to the TabAndroid
// object associated with `tab`.
void AttachTabLabel(TabAndroid* tab,
                    std::string_view guid,
                    std::string_view device_name);

// Calls the Java SendTabToSelfAndroidBridge to display the message banner. This
// is called upon successful auto-opening of the received tab(s) in the
// background. `web_contents` is the *currently-active* tab, *not* one of the
// auto-opened ones. `opened_tab_url` is only meaningful in the single-tab case.
void ShowMessageBanner(content::WebContents* web_contents,
                       std::string_view device_name,
                       int opened_tab_count,
                       const GURL& opened_tab_url);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ANDROID_BRIDGE_H_
