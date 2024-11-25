// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/web_view_side_panel_throttle.h"

#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/base/window_open_disposition.h"

const char kWebViewSidePanelWebContentsUserDataKey[] =
    "web_view_side_panel_web_contents_user_data";

WebViewSidePanelWebContentsUserData::WebViewSidePanelWebContentsUserData(
    base::WeakPtr<Delegate> delegate)
    : delegate_(delegate) {}

WebViewSidePanelWebContentsUserData::~WebViewSidePanelWebContentsUserData() =
    default;

std::unique_ptr<content::NavigationThrottle>
MaybeCreateWebViewSidePanelThrottleFor(content::NavigationHandle* handle) {
  // Only install throttle for WebContents that are in the WebViewSidePanel.
  if (!handle || !handle->IsInPrimaryMainFrame() || !handle->GetWebContents() ||
      !handle->GetWebContents()->GetUserData(
          kWebViewSidePanelWebContentsUserDataKey)) {
    return nullptr;
  }
  return std::make_unique<navigation_interception::InterceptNavigationThrottle>(
      handle, base::BindRepeating([](content::NavigationHandle* handle) {
        DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
        auto* data = static_cast<WebViewSidePanelWebContentsUserData*>(
            handle->GetWebContents()->GetUserData(
                kWebViewSidePanelWebContentsUserDataKey));
        // The delegate is stored in a WeakPtr. Check if it is still there.
        if (!data->delegate())
          return true;
        if (data->delegate()->IsNavigationAllowed(
                handle->GetURL(), handle->GetWebContents()->GetURL()))
          return false;

        content::OpenURLParams params(
            handle->GetURL(), content::Referrer(handle->GetReferrer()),
            WindowOpenDisposition::NEW_FOREGROUND_TAB,
            handle->GetPageTransition(), handle->IsRendererInitiated());
        params.initiator_origin = handle->GetInitiatorOrigin();
        params.initiator_base_url = handle->GetInitiatorBaseUrl();
        data->delegate()->OpenUrlInBrowser(params);
        return true;
      }),
      navigation_interception::SynchronyMode::kSync);
}
