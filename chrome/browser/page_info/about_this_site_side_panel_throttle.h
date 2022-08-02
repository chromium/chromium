// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_THROTTLE_H_
#define CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_THROTTLE_H_

#include "base/callback.h"
#include "base/supports_user_data.h"

namespace content {
struct OpenURLParams;
class NavigationHandle;
class NavigationThrottle;
}  // namespace content

extern const char kAboutThisSiteWebContentsUserDataKey[];

// Holds a handler to open a URL in a new tab in the browser that the sidepanel
// of this webcontents is associated with. The NavigationThrottle from
// |MaybeCreateAboutThisSiteThrottleFor| will check if this UserData is present
// and if it is present intercept cross-origin navigations and open them using
// the handler.
struct AboutThisSiteWebContentsUserData : public base::SupportsUserData::Data {
  explicit AboutThisSiteWebContentsUserData(
      base::RepeatingCallback<void(const content::OpenURLParams&)> handler);
  ~AboutThisSiteWebContentsUserData() override;

  base::RepeatingCallback<void(const content::OpenURLParams&)>
      open_in_new_tab_handler;
};

// Installs a NavigationThrottle if an AboutThisSiteWebContentsUserData is
// associated with the WebContents of this navigation.
std::unique_ptr<content::NavigationThrottle>
MaybeCreateAboutThisSiteThrottleFor(content::NavigationHandle* handle);

#endif  // CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_THROTTLE_H_
