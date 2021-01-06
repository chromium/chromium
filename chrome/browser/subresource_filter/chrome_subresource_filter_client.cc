// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/subresource_filter/content/browser/ads_intervention_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/profile_interaction_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if defined(OS_ANDROID)
#include "components/subresource_filter/android/ads_blocked_infobar_delegate.h"
#endif

ChromeSubresourceFilterClient::ChromeSubresourceFilterClient(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents_);
  profile_context_ = SubresourceFilterProfileContextFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  profile_interaction_manager_ =
      std::make_unique<subresource_filter::ProfileInteractionManager>(
          web_contents, profile_context_);
}

ChromeSubresourceFilterClient::~ChromeSubresourceFilterClient() = default;

// static
void ChromeSubresourceFilterClient::
    CreateThrottleManagerWithClientForWebContents(
        content::WebContents* web_contents) {
  subresource_filter::RulesetService* ruleset_service =
      g_browser_process->subresource_filter_ruleset_service();
  subresource_filter::VerifiedRulesetDealer::Handle* dealer =
      ruleset_service ? ruleset_service->GetRulesetDealer() : nullptr;
  subresource_filter::ContentSubresourceFilterThrottleManager::
      CreateForWebContents(
          web_contents,
          std::make_unique<ChromeSubresourceFilterClient>(web_contents),
          dealer);
}

// static
ChromeSubresourceFilterClient* ChromeSubresourceFilterClient::FromWebContents(
    content::WebContents* web_contents) {
  auto* throttle_manager = subresource_filter::
      ContentSubresourceFilterThrottleManager::FromWebContents(web_contents);

  if (!throttle_manager)
    return nullptr;

  return static_cast<ChromeSubresourceFilterClient*>(
      throttle_manager->client());
}

void ChromeSubresourceFilterClient::OnReloadRequested() {
  // TODO(crbug.com/1116095): Once ContentSubresourceFilterThrottleManager knows
  // about ProfileInteractionManager, this method can move entirely into
  // ContentSubresourceFilterThrottleManager::OnReloadRequested() and
  // SubresourceFilterClient::OnReloadRequested() can be eliminated.
  profile_interaction_manager_->OnReloadRequested();
}

void ChromeSubresourceFilterClient::ShowNotification() {
  const GURL& top_level_url = web_contents_->GetLastCommittedURL();
  if (profile_context_->settings_manager()->ShouldShowUIForSite(
          top_level_url)) {
    ShowUI(top_level_url);
  } else {
    subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
        subresource_filter::SubresourceFilterAction::kUISuppressed);
  }
}

subresource_filter::mojom::ActivationLevel
ChromeSubresourceFilterClient::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    subresource_filter::mojom::ActivationLevel initial_activation_level,
    subresource_filter::ActivationDecision* decision) {
  // TODO(crbug.com/1116095): Once SafeBrowsingActivationThrottle knows about
  // ProfileInteractionManager, it can invoke ProfileInteractionManager directly
  // and SubresourceFilterClient::OnPageActivationComputed() can be eliminated.
  return profile_interaction_manager_->OnPageActivationComputed(
      navigation_handle, initial_activation_level, decision);
}

void ChromeSubresourceFilterClient::OnAdsViolationTriggered(
    content::RenderFrameHost* rfh,
    subresource_filter::mojom::AdsViolation triggered_violation) {
  // TODO(crbug.com/1116095): Once ContentSubresourceFilterThrottleManager knows
  // about ProfileInteractionManager, it can invoke the
  // ProfileInteractionManager directly and
  // SubresourceFilterClient::OnAdsViolationTriggered() can be eliminated.
  profile_interaction_manager_->OnAdsViolationTriggered(rfh,
                                                        triggered_violation);
}

const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
ChromeSubresourceFilterClient::GetSafeBrowsingDatabaseManager() {
  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();
  return safe_browsing_service ? safe_browsing_service->database_manager()
                               : nullptr;
}

void ChromeSubresourceFilterClient::ShowUI(const GURL& url) {
#if defined(OS_ANDROID)
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  subresource_filter::AdsBlockedInfobarDelegate::Create(infobar_service);
#endif
  // TODO(https://crbug.com/1103176): Plumb the actual frame reference here
  // (it comes  from
  // ContentSubresourceFilterThrottleManager::DidDisallowFirstSubresource, which
  // comes from a specific frame).
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents_->GetMainFrame());
  content_settings->OnContentBlocked(ContentSettingsType::ADS);

  subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
      subresource_filter::SubresourceFilterAction::kUIShown);
  profile_context_->settings_manager()->OnDidShowUI(url);
}
