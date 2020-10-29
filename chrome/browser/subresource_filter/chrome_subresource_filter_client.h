// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_SUBRESOURCE_FILTER_CLIENT_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_SUBRESOURCE_FILTER_CLIENT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace subresource_filter {
class ContentSubresourceFilterThrottleManager;
class SubresourceFilterProfileContext;
}  // namespace subresource_filter

// Chrome implementation of SubresourceFilterClient. Instances are associated
// with and owned by ContentSubresourceFilterThrottleManager instances.
class ChromeSubresourceFilterClient
    : public content::WebContentsObserver,
      public subresource_filter::SubresourceFilterClient {
 public:
  explicit ChromeSubresourceFilterClient(content::WebContents* web_contents);
  ~ChromeSubresourceFilterClient() override;

  // Creates a ContentSubresourceFilterThrottleManager and attaches it to
  // |web_contents|, passing it an instance of this client and other
  // embedder-level state.
  static void CreateThrottleManagerWithClientForWebContents(
      content::WebContents* web_contents);

  // Returns the ChromeSubresourceFilterClient instance that is owned by the
  // ThrottleManager owned by |web_contents|, or nullptr if there is no such
  // ThrottleManager.
  static ChromeSubresourceFilterClient* FromWebContents(
      content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // SubresourceFilterClient:
  void ShowNotification() override;
  subresource_filter::mojom::ActivationLevel OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      subresource_filter::mojom::ActivationLevel initial_activation_level,
      subresource_filter::ActivationDecision* decision) override;
  void OnAdsViolationTriggered(
      content::RenderFrameHost* rfh,
      subresource_filter::mojom::AdsViolation triggered_violation) override;
  const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
  GetSafeBrowsingDatabaseManager() override;
  void OnReloadRequested() override;

  // Should be called by devtools in response to a protocol command to enable ad
  // blocking in this WebContents. Should only persist while devtools is
  // attached.
  void ToggleForceActivationInCurrentWebContents(bool force_activation);

  bool did_show_ui_for_navigation() const {
    return did_show_ui_for_navigation_;
  }

 private:
  void AllowlistByContentSettings(const GURL& url);
  void ShowUI(const GURL& url);

  std::unique_ptr<subresource_filter::ContentSubresourceFilterThrottleManager>
      throttle_manager_;

  // Owned by the profile.
  subresource_filter::SubresourceFilterProfileContext* profile_context_ =
      nullptr;

  bool did_show_ui_for_navigation_ = false;

  bool ads_violation_triggered_for_last_committed_navigation_ = false;

  // Corresponds to a devtools command which triggers filtering on all page
  // loads. We must be careful to ensure this boolean does not persist after the
  // devtools window is closed, which should be handled by the devtools system.
  bool activated_via_devtools_ = false;

  DISALLOW_COPY_AND_ASSIGN(ChromeSubresourceFilterClient);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_SUBRESOURCE_FILTER_CLIENT_H_
