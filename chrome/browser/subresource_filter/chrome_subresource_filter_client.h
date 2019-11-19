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
#include "content/public/browser/web_contents_user_data.h"

class GURL;
class SubresourceFilterContentSettingsManager;

namespace content {
class NavigationHandle;
class NavigationThrottle;
class WebContents;
}  // namespace content

namespace subresource_filter {
class ContentSubresourceFilterThrottleManager;
}  // namespace subresource_filter

// This enum backs a histogram. Make sure new elements are only added to the
// end. Keep histograms.xml up to date with any changes.
enum class SubresourceFilterAction {
  // Standard UI shown. On Desktop this is in the omnibox,
  // On Android, it is an infobar.
  kUIShown = 0,

  // The UI was suppressed due to "smart" logic which tries not to spam the UI
  // on navigations on the same origin within a certain time.
  kUISuppressed = 1,

  // On Desktop, this is a bubble. On Android it is an
  // expanded infobar.
  kDetailsShown = 2,

  kClickedLearnMore = 3,

  // Logged when the user presses "Always allow ads" scoped to a particular
  // site. Does not count manual changes to content settings.
  kWhitelistedSite = 4,

  // Logged when a devtools message arrives notifying us to force activation in
  // this web contents.
  kForcedActivationEnabled = 5,

  kMaxValue = kForcedActivationEnabled
};

// Chrome implementation of SubresourceFilterClient.
class ChromeSubresourceFilterClient
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ChromeSubresourceFilterClient>,
      public subresource_filter::SubresourceFilterClient {
 public:
  explicit ChromeSubresourceFilterClient(content::WebContents* web_contents);
  ~ChromeSubresourceFilterClient() override;

  void MaybeAppendNavigationThrottles(
      content::NavigationHandle* navigation_handle,
      std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles);

  void OnReloadRequested();

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // SubresourceFilterClient:
  void ShowNotification() override;
  subresource_filter::mojom::ActivationLevel OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      subresource_filter::mojom::ActivationLevel initial_activation_level,
      subresource_filter::ActivationDecision* decision) override;

  // Should be called by devtools in response to a protocol command to enable ad
  // blocking in this WebContents. Should only persist while devtools is
  // attached.
  void ToggleForceActivationInCurrentWebContents(bool force_activation);

  bool did_show_ui_for_navigation() const {
    return did_show_ui_for_navigation_;
  }

  const subresource_filter::ContentSubresourceFilterThrottleManager*
  GetThrottleManager() const;

  static void LogAction(SubresourceFilterAction action);

 private:
  friend class content::WebContentsUserData<ChromeSubresourceFilterClient>;
  void WhitelistByContentSettings(const GURL& url);
  void ShowUI(const GURL& url);

  std::unique_ptr<subresource_filter::ContentSubresourceFilterThrottleManager>
      throttle_manager_;

  // Owned by the profile.
  SubresourceFilterContentSettingsManager* settings_manager_ = nullptr;

  bool did_show_ui_for_navigation_ = false;

  // Corresponds to a devtools command which triggers filtering on all page
  // loads. We must be careful to ensure this boolean does not persist after the
  // devtools window is closed, which should be handled by the devtools system.
  bool activated_via_devtools_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ChromeSubresourceFilterClient);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_SUBRESOURCE_FILTER_CLIENT_H_
