// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

class GURL;

namespace content {
class WebContents;
}

namespace send_tab_to_self {

class SendTabToSelfSyncService;

// Returns true if the feature should be offered in menus.
bool ShouldOfferFeature(content::WebContents* web_contents);

// |send_tab_to_self_sync_service| can be null (in incognito or guest profile).
bool ShouldOfferToShareUrl(
    SendTabToSelfSyncService* send_tab_to_self_sync_service,
    const GURL& url);

// Returns true if the omnibox icon for the feature should be offered.
bool ShouldOfferOmniboxIcon(content::WebContents* web_contents);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
