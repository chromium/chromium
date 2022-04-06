// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

namespace content {
class WebContents;
}

namespace send_tab_to_self {

// Returns true if the feature should be offered in menus.
bool ShouldOfferFeature(content::WebContents* web_contents);

// Returns true if the omnibox icon for the feature should be offered.
bool ShouldOfferOmniboxIcon(content::WebContents* web_contents);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
