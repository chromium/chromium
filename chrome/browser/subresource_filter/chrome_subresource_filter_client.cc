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
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/infobars/chrome_confirm_infobar.h"
#include "components/subresource_filter/android/ads_blocked_infobar_delegate.h"
#endif

ChromeSubresourceFilterClient::ChromeSubresourceFilterClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(web_contents);
  profile_context_ = SubresourceFilterProfileContextFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
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

void ChromeSubresourceFilterClient::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    // TODO(csharrison): This should probably be reset at commit time, not at
    // navigation start.
    did_show_ui_for_navigation_ = false;
  }
}

void ChromeSubresourceFilterClient::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() && navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    ads_violation_triggered_for_last_committed_navigation_ = false;
  }
}

void ChromeSubresourceFilterClient::OnReloadRequested() {
  // TODO(crbug.com/1116095): Once ContentSubresourceFilterThrottleManager knows
  // about content settings, this method can move entirely into
  // ContentSubresourceFilterThrottleManager::OnReloadRequested() and
  // SubresourceFilterClient::OnReloadRequested() can be eliminated.
  subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
      subresource_filter::SubresourceFilterAction::kAllowlistedSite);
  AllowlistByContentSettings(web_contents()->GetLastCommittedURL());
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

void ChromeSubresourceFilterClient::ShowNotification() {
  if (did_show_ui_for_navigation_)
    return;

  const GURL& top_level_url = web_contents()->GetLastCommittedURL();
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
  DCHECK(navigation_handle->IsInMainFrame());

  subresource_filter::mojom::ActivationLevel effective_activation_level =
      initial_activation_level;
  if (activated_via_devtools_) {
    effective_activation_level =
        subresource_filter::mojom::ActivationLevel::kEnabled;
    *decision = subresource_filter::ActivationDecision::FORCED_ACTIVATION;
  }

  if (profile_context_->ads_intervention_manager()->ShouldActivate(
          navigation_handle)) {
    effective_activation_level =
        subresource_filter::mojom::ActivationLevel::kEnabled;
    *decision = subresource_filter::ActivationDecision::ACTIVATED;
  }

  const GURL& url(navigation_handle->GetURL());
  if (url.SchemeIsHTTPOrHTTPS()) {
    profile_context_->settings_manager()->SetSiteMetadataBasedOnActivation(
        url,
        effective_activation_level ==
            subresource_filter::mojom::ActivationLevel::kEnabled,
        subresource_filter::SubresourceFilterContentSettingsManager::
            ActivationSource::kSafeBrowsing);
  }

  if (profile_context_->settings_manager()->GetSitePermission(url) ==
      CONTENT_SETTING_ALLOW) {
    if (effective_activation_level ==
        subresource_filter::mojom::ActivationLevel::kEnabled) {
      *decision = subresource_filter::ActivationDecision::URL_ALLOWLISTED;
    }
    return subresource_filter::mojom::ActivationLevel::kDisabled;
  }

  return effective_activation_level;
}

// TODO(https://crbug.com/1131969): Consider adding reporting when
// ads violations are triggered.
void ChromeSubresourceFilterClient::OnAdsViolationTriggered(
    content::RenderFrameHost* rfh,
    subresource_filter::mojom::AdsViolation triggered_violation) {
  // Only trigger violations once per navigation. The ads intervention
  // manager ignores all interventions after recording an intervention
  // for the intervention duration, however, a page that began a navigation
  // before the intervention duration and was still alive after the duration
  // could re-trigger an ads intervention.
  if (ads_violation_triggered_for_last_committed_navigation_)
    return;

  // If the feature is disabled, simulate ads interventions as if we were
  // enforcing on ads: do not record new interventions if we would be enforcing
  // an intervention on ads already.
  //
  // TODO(https://crbug.com/1131971): Add support for enabling ads interventions
  // separately for different ads violations.
  const GURL& url = rfh->GetLastCommittedURL();
  base::Optional<
      subresource_filter::AdsInterventionManager::LastAdsIntervention>
      last_intervention =
          profile_context_->ads_intervention_manager()->GetLastAdsIntervention(
              url);
  if (last_intervention &&
      last_intervention->duration_since <
          subresource_filter::kAdsInterventionDuration.Get())
    return;

  profile_context_->ads_intervention_manager()
      ->TriggerAdsInterventionForUrlOnSubsequentLoads(url, triggered_violation);

  ads_violation_triggered_for_last_committed_navigation_ = true;
}

void ChromeSubresourceFilterClient::AllowlistByContentSettings(
    const GURL& top_level_url) {
  profile_context_->settings_manager()->AllowlistSite(top_level_url);
}

const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
ChromeSubresourceFilterClient::GetSafeBrowsingDatabaseManager() {
  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();
  return safe_browsing_service ? safe_browsing_service->database_manager()
                               : nullptr;
}

void ChromeSubresourceFilterClient::ToggleForceActivationInCurrentWebContents(
    bool force_activation) {
  if (!activated_via_devtools_ && force_activation)
    subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
        subresource_filter::SubresourceFilterAction::kForcedActivationEnabled);
  activated_via_devtools_ = force_activation;
}

void ChromeSubresourceFilterClient::ShowUI(const GURL& url) {
#if defined(OS_ANDROID)
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  subresource_filter::AdsBlockedInfobarDelegate::Create(
      infobar_service, ChromeConfirmInfoBar::GetResourceIdMapper());
#endif
  // TODO(https://crbug.com/1103176): Plumb the actual frame reference here
  // (it comes  from
  // ContentSubresourceFilterThrottleManager::DidDisallowFirstSubresource, which
  // comes from a specific frame).
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetMainFrame());
  content_settings->OnContentBlocked(ContentSettingsType::ADS);

  subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
      subresource_filter::SubresourceFilterAction::kUIShown);
  did_show_ui_for_navigation_ = true;
  profile_context_->settings_manager()->OnDidShowUI(url);
}
