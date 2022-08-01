// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/about_this_site_side_panel_throttle.h"

#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"

const char kAboutThisSiteWebContentsUserDataKey[] =
    "about_this_site_web_contents_user_data";

AboutThisSiteWebContentsUserData::AboutThisSiteWebContentsUserData(
    base::RepeatingCallback<void(const content::OpenURLParams&)> handler)
    : open_in_new_tab_handler(handler) {}

AboutThisSiteWebContentsUserData::~AboutThisSiteWebContentsUserData() = default;

std::unique_ptr<content::NavigationThrottle>
MaybeCreateAboutThisSiteThrottleFor(content::NavigationHandle* handle) {
  // Only install throttle for WebContents that are in the SidePanel.
  if (!handle || !handle->IsInPrimaryMainFrame() || !handle->GetWebContents() ||
      !handle->GetWebContents()->GetUserData(
          kAboutThisSiteWebContentsUserDataKey)) {
    return nullptr;
  }
  return std::make_unique<navigation_interception::InterceptNavigationThrottle>(
      handle, base::BindRepeating([](content::NavigationHandle* handle) {
        if (url::IsSameOriginWith(handle->GetURL(),
                                  handle->GetWebContents()->GetURL()))
          return false;
        // Cross-origin navigations should be moved to a regular tab and can't
        // be handled in the side-panel.
        content::OpenURLParams params(
            handle->GetURL(), content::Referrer(handle->GetReferrer()),
            WindowOpenDisposition::NEW_FOREGROUND_TAB,
            handle->GetPageTransition(), handle->IsRendererInitiated());
        params.initiator_origin = handle->GetInitiatorOrigin();

        auto* data = static_cast<AboutThisSiteWebContentsUserData*>(
            handle->GetWebContents()->GetUserData(
                kAboutThisSiteWebContentsUserDataKey));
        data->open_in_new_tab_handler.Run(params);
        return true;
      }),
      navigation_interception::SynchronyMode::kSync);
}
