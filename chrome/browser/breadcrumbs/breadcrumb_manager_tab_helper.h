// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_
#define CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_

#include "components/breadcrumbs/core/breadcrumb_manager_tab_helper.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

// Handles logging of Breadcrumb events associated with |web_contents_|.
class BreadcrumbManagerTabHelper
    : public breadcrumbs::BreadcrumbManagerTabHelper,
      public content::WebContentsObserver,
      public content::WebContentsUserData<BreadcrumbManagerTabHelper> {
 public:
  ~BreadcrumbManagerTabHelper() override;
  BreadcrumbManagerTabHelper(const BreadcrumbManagerTabHelper&) = delete;
  BreadcrumbManagerTabHelper& operator=(const BreadcrumbManagerTabHelper&) =
      delete;

 private:
  friend class content::WebContentsUserData<BreadcrumbManagerTabHelper>;

  explicit BreadcrumbManagerTabHelper(content::WebContents* web_contents);

  // breadcrumbs::BreadcrumbManagerTabHelper:
  void PlatformLogEvent(const std::string& event) override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void DidChangeVisibleSecurityState() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_
