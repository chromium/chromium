// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_
#define CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_

#include <optional>

#include "components/breadcrumbs/core/breadcrumb_manager_tab_helper.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

// Handles logging of Breadcrumb events associated with |web_contents_|.
class BreadcrumbManagerTabHelper
    : public breadcrumbs::BreadcrumbManagerTabHelper,
      public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(BreadcrumbManagerTabHelper);

  BreadcrumbManagerTabHelper(
      tabs::TabInterface& tab,
      content::WebContents* web_contents,
      std::optional<int> previous_unique_id = std::nullopt);
  ~BreadcrumbManagerTabHelper() override;
  BreadcrumbManagerTabHelper(const BreadcrumbManagerTabHelper&) = delete;
  BreadcrumbManagerTabHelper& operator=(const BreadcrumbManagerTabHelper&) =
      delete;

  static BreadcrumbManagerTabHelper* From(tabs::TabInterface* tab);
  static const BreadcrumbManagerTabHelper* From(const tabs::TabInterface* tab);

  std::optional<int> GetPreviousUniqueId() const { return previous_unique_id_; }

 private:
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

  const std::optional<int> previous_unique_id_;
  ui::ScopedUnownedUserData<BreadcrumbManagerTabHelper>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_
