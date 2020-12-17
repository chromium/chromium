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

class GURL;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace subresource_filter {
class ContentSubresourceFilterThrottleManager;
class ProfileInteractionManager;
class SubresourceFilterProfileContext;
}  // namespace subresource_filter

// Chrome implementation of SubresourceFilterClient. Instances are associated
// with and owned by ContentSubresourceFilterThrottleManager instances.
class ChromeSubresourceFilterClient
    : public subresource_filter::SubresourceFilterClient {
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

 private:
  void ShowUI(const GURL& url);

  content::WebContents* web_contents_;

  std::unique_ptr<subresource_filter::ContentSubresourceFilterThrottleManager>
      throttle_manager_;

  // Owned by the profile.
  subresource_filter::SubresourceFilterProfileContext* profile_context_ =
      nullptr;

  std::unique_ptr<subresource_filter::ProfileInteractionManager>
      profile_interaction_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSubresourceFilterClient);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_SUBRESOURCE_FILTER_CLIENT_H_
