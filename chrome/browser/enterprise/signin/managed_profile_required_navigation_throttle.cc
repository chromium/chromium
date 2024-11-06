// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/managed_profile_required_navigation_throttle.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/supports_user_data.h"
#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_controller_client.h"
#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_page.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace {

class BlockingInfo : public base::SupportsUserData::Data {
 public:
  explicit BlockingInfo(content::WebContents* allowed_web_contents)
      : allowed_web_contents_(allowed_web_contents) {}

  BlockingInfo(const BlockingInfo&) = delete;
  BlockingInfo& operator=(const BlockingInfo&) = delete;
  ~BlockingInfo() override = default;

  const content::WebContents* allowed_web_contents() const {
    return allowed_web_contents_.get();
  }

 private:
  const raw_ptr<content::WebContents> allowed_web_contents_;
};

// BrowserContexts that are marked with this UserData key should have all
// navigations throttled.
const void* const kNavigationBlockedForManagedProfileCreationInfo =
    &kNavigationBlockedForManagedProfileCreationInfo;

}  // namespace

// static
std::unique_ptr<ManagedProfileRequiredNavigationThrottle>
ManagedProfileRequiredNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(
          features::kManagedProfileRequiredInterstitial)) {
    return nullptr;
  }

  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return nullptr;
  }

  auto* navigation_blocked_for_managed_profile_creation_info =
      navigation_handle->GetWebContents()->GetBrowserContext()->GetUserData(
          kNavigationBlockedForManagedProfileCreationInfo);

  // If nothing should be blocked, continue.
  if (!navigation_blocked_for_managed_profile_creation_info) {
    return nullptr;
  }
  BlockingInfo* blocking_info = static_cast<BlockingInfo*>(
      navigation_blocked_for_managed_profile_creation_info);
  CHECK(blocking_info);

  if (blocking_info->allowed_web_contents() ==
      navigation_handle->GetWebContents()) {
    return nullptr;
  }
  return std::make_unique<ManagedProfileRequiredNavigationThrottle>(
      navigation_handle);
}

ManagedProfileRequiredNavigationThrottle::
    ManagedProfileRequiredNavigationThrottle(
        content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

ManagedProfileRequiredNavigationThrottle::
    ~ManagedProfileRequiredNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ManagedProfileRequiredNavigationThrottle::WillStartRequest() {
  return ProcessThrottleEvent();
}

content::NavigationThrottle::ThrottleCheckResult
ManagedProfileRequiredNavigationThrottle::WillRedirectRequest() {
  return ProcessThrottleEvent();
}

content::NavigationThrottle::ThrottleCheckResult
ManagedProfileRequiredNavigationThrottle::WillProcessResponse() {
  return ProcessThrottleEvent();
}

content::NavigationThrottle::ThrottleCheckResult
ManagedProfileRequiredNavigationThrottle::WillFailRequest() {
  return ProcessThrottleEvent();
}

content::NavigationThrottle::ThrottleCheckResult
ManagedProfileRequiredNavigationThrottle::ProcessThrottleEvent() {
  auto* navigation_blocked_for_managed_profile_creation_info =
      navigation_handle()->GetWebContents()->GetBrowserContext()->GetUserData(
          kNavigationBlockedForManagedProfileCreationInfo);

  // If nothing should be blocked, continue.
  if (!navigation_blocked_for_managed_profile_creation_info) {
    return PROCEED;
  }
  auto managed_profile_required = std::make_unique<ManagedProfileRequiredPage>(
      navigation_handle()->GetWebContents(), navigation_handle()->GetURL(),
      std::make_unique<ManagedProfileRequiredControllerClient>(
          navigation_handle()->GetWebContents(),
          navigation_handle()->GetURL()));

  std::string error_page_content = managed_profile_required->GetHTMLContents();
  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      navigation_handle(), std::move(managed_profile_required));
  return content::NavigationThrottle::ThrottleCheckResult(
      CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content);
}

const char* ManagedProfileRequiredNavigationThrottle::GetNameForLogging() {
  return "ManagedProfileRequiredNavigationThrottle";
}

// static
base::ScopedClosureRunner ManagedProfileRequiredNavigationThrottle::
    BlockNavigationUntilEnterpriseActionTaken(
        content::BrowserContext* browser_context,
        content::WebContents* allowed_web_contents) {
  browser_context->SetUserData(
      kNavigationBlockedForManagedProfileCreationInfo,
      std::make_unique<BlockingInfo>(allowed_web_contents));
  return base::ScopedClosureRunner(base::BindOnce(
      &content::BrowserContext::RemoveUserData, browser_context->GetWeakPtr(),
      kNavigationBlockedForManagedProfileCreationInfo));
}

//  static
bool ManagedProfileRequiredNavigationThrottle::IsBlockingNavigations(
    content::BrowserContext* browser_context) {
  return browser_context->GetUserData(
             kNavigationBlockedForManagedProfileCreationInfo) != nullptr;
}
