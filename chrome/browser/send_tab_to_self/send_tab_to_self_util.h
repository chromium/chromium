// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace send_tab_to_self {

// Returns true if the SendTabToSelf sync datatype is active.
bool IsUserSyncTypeActive(Profile* profile);

// Returns true if the user has one or more valid device to share to.
bool HasValidTargetDevice(Profile* profile);

// Returns true if the tab and web content requirements are met:
//  User is viewing an HTTP or HTTPS page.
//  User is not on a native page.
//  User is not in Incongnito mode.
bool AreContentRequirementsMet(const GURL& gurl, Profile* profile);

// Returns true if the feature should be offered in menus.
bool ShouldOfferFeature(content::WebContents* web_contents);

// Returns true if the feature should be offered in link context menus.
bool ShouldOfferFeatureForLink(content::WebContents* web_contents,
                               const GURL& link_url);

// Returns true if the omnibox icon for the feature should be offered.
bool ShouldOfferOmniboxIcon(content::WebContents* web_contents);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
