// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/web_view_side_panel_throttle.h"

#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/base/window_open_disposition.h"

const char kWebViewSidePanelWebContentsUserDataKey[] =
    "web_view_side_panel_web_contents_user_data";

namespace {

bool CanNavigateInPanel(content::NavigationHandle* handle,
                        const GURL& observed_url,
                        const GURL& next_url) {
  // Server redirects of the observed_url URL are allowed to stay in the
  // SidePanel.
  const GURL& original_url = handle->GetRedirectChain().front();
  if (handle->WasServerRedirect() && original_url == observed_url) {
    return true;
  }
  // Same URL navigations are allowed to stay in the SidePanel.
  const GURL& current_url = handle->GetWebContents()->GetURL();
  if (next_url == current_url) {
    return true;
  }

  return false;
}

}  // namespace

WebViewSidePanelWebContentsUserData::WebViewSidePanelWebContentsUserData(
    base::WeakPtr<Delegate> delegate)
    : delegate_(delegate) {}

WebViewSidePanelWebContentsUserData::~WebViewSidePanelWebContentsUserData() =
    default;

void MaybeCreateAndAddWebViewSidePanelThrottle(
    content::NavigationThrottleRegistry& registry) {
  // Only install throttle for WebContents that are in the WebViewSidePanel.
  auto& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame() || !handle.GetWebContents() ||
      !handle.GetWebContents()->GetUserData(
          kWebViewSidePanelWebContentsUserDataKey)) {
    return;
  }
  const GURL& observed_url = handle.GetURL();
  registry.AddThrottle(
      std::make_unique<navigation_interception::InterceptNavigationThrottle>(
          registry,
          base::BindRepeating(
              [](const GURL& observed_url, content::NavigationHandle* handle,
                 bool should_run_async,
                 navigation_interception::InterceptNavigationThrottle::
                     ResultCallback result_callback) {
                DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
                CHECK(!should_run_async);
                auto* data = static_cast<WebViewSidePanelWebContentsUserData*>(
                    handle->GetWebContents()->GetUserData(
                        kWebViewSidePanelWebContentsUserDataKey));
                // The delegate is stored in a WeakPtr. Check if it is still
                // there.
                if (!data->delegate()) {
                  std::move(result_callback).Run(true);
                  return;
                }
                const GURL& next_url = handle->GetURL();
                if (CanNavigateInPanel(handle, observed_url, next_url)) {
                  std::move(result_callback).Run(false);
                  return;
                }

                content::OpenURLParams params(
                    next_url, content::Referrer(handle->GetReferrer()),
                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                    handle->GetPageTransition(), handle->IsRendererInitiated());
                params.initiator_origin = handle->GetInitiatorOrigin();
                params.initiator_base_url = handle->GetInitiatorBaseUrl();
                data->delegate()->OpenUrlInBrowser(params);
                std::move(result_callback).Run(true);
              },
              observed_url),
          navigation_interception::SynchronyMode::kSync, std::nullopt));
}
