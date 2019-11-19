// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/content_settings/ads_blocked_infobar_delegate.h"
#endif

ChromeSubresourceFilterClient::ChromeSubresourceFilterClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(web_contents);
  SubresourceFilterProfileContext* context =
      SubresourceFilterProfileContextFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  settings_manager_ = context->settings_manager();

  subresource_filter::RulesetService* ruleset_service =
      g_browser_process->subresource_filter_ruleset_service();
  subresource_filter::VerifiedRulesetDealer::Handle* dealer =
      ruleset_service ? ruleset_service->GetRulesetDealer() : nullptr;
  throttle_manager_ = std::make_unique<
      subresource_filter::ContentSubresourceFilterThrottleManager>(
      this, dealer, web_contents);
}

ChromeSubresourceFilterClient::~ChromeSubresourceFilterClient() {}

void ChromeSubresourceFilterClient::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    // TODO(csharrison): This should probably be reset at commit time, not at
    // navigation start.
    did_show_ui_for_navigation_ = false;
  }
}

void ChromeSubresourceFilterClient::MaybeAppendNavigationThrottles(
    content::NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();
  if (navigation_handle->IsInMainFrame() && safe_browsing_service) {
    throttles->push_back(
        std::make_unique<subresource_filter::
                             SubresourceFilterSafeBrowsingActivationThrottle>(
            navigation_handle, this,
            base::CreateSingleThreadTaskRunner({content::BrowserThread::IO}),
            safe_browsing_service->database_manager()));
  }

  throttle_manager_->MaybeAppendNavigationThrottles(navigation_handle,
                                                    throttles);
}

void ChromeSubresourceFilterClient::OnReloadRequested() {
  LogAction(SubresourceFilterAction::kWhitelistedSite);
  WhitelistByContentSettings(web_contents()->GetLastCommittedURL());
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

void ChromeSubresourceFilterClient::ShowNotification() {
  if (did_show_ui_for_navigation_)
    return;

  const GURL& top_level_url = web_contents()->GetLastCommittedURL();
  if (settings_manager_->ShouldShowUIForSite(top_level_url)) {
    ShowUI(top_level_url);
  } else {
    LogAction(SubresourceFilterAction::kUISuppressed);
  }
}

subresource_filter::mojom::ActivationLevel
ChromeSubresourceFilterClient::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    subresource_filter::mojom::ActivationLevel initial_activation_level,
    subresource_filter::ActivationDecision* decision) {
  DCHECK(navigation_handle->IsInMainFrame());

  subresource_filter::mojom::ActivationLevel effective_activation_level =
      initial_activation_level;
  if (activated_via_devtools_) {
    effective_activation_level =
        subresource_filter::mojom::ActivationLevel::kEnabled;
    *decision = subresource_filter::ActivationDecision::FORCED_ACTIVATION;
  }

  const GURL& url(navigation_handle->GetURL());
  if (url.SchemeIsHTTPOrHTTPS()) {
    settings_manager_->ResetSiteMetadataBasedOnActivation(
        url, effective_activation_level ==
                 subresource_filter::mojom::ActivationLevel::kEnabled);
  }

  if (settings_manager_->GetSitePermission(url) == CONTENT_SETTING_ALLOW) {
    if (effective_activation_level ==
        subresource_filter::mojom::ActivationLevel::kEnabled) {
      *decision = subresource_filter::ActivationDecision::URL_WHITELISTED;
    }
    return subresource_filter::mojom::ActivationLevel::kDisabled;
  }
  return effective_activation_level;
}

void ChromeSubresourceFilterClient::WhitelistByContentSettings(
    const GURL& top_level_url) {
  settings_manager_->WhitelistSite(top_level_url);
}

void ChromeSubresourceFilterClient::ToggleForceActivationInCurrentWebContents(
    bool force_activation) {
  if (!activated_via_devtools_ && force_activation)
    LogAction(SubresourceFilterAction::kForcedActivationEnabled);
  activated_via_devtools_ = force_activation;
}

const subresource_filter::ContentSubresourceFilterThrottleManager*
ChromeSubresourceFilterClient::GetThrottleManager() const {
  return throttle_manager_.get();
}

// static
void ChromeSubresourceFilterClient::LogAction(SubresourceFilterAction action) {
  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.Actions2", action);
}

void ChromeSubresourceFilterClient::ShowUI(const GURL& url) {
#if defined(OS_ANDROID)
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  AdsBlockedInfobarDelegate::Create(infobar_service);
#endif
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(ContentSettingsType::ADS);

  LogAction(SubresourceFilterAction::kUIShown);
  did_show_ui_for_navigation_ = true;
  settings_manager_->OnDidShowUI(url);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeSubresourceFilterClient)
