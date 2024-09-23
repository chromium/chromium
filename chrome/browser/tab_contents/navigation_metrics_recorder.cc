// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/experiment_manager_impl.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/privacy_sandbox/tpcd_utils.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_features.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

NavigationMetricsRecorder::NavigationMetricsRecorder(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<NavigationMetricsRecorder>(*web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  site_engagement_service_ =
      site_engagement::SiteEngagementService::Get(profile);
  cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);

}

NavigationMetricsRecorder::~NavigationMetricsRecorder() = default;

ThirdPartyCookieBlockState
NavigationMetricsRecorder::GetThirdPartyCookieBlockState(const GURL& url) {
  if (!cookie_settings_->ShouldBlockThirdPartyCookies())
    return ThirdPartyCookieBlockState::kCookiesAllowed;
  bool blocking_enabled_for_site =
      !cookie_settings_->IsThirdPartyAccessAllowed(url);
  return blocking_enabled_for_site
             ? ThirdPartyCookieBlockState::kThirdPartyCookiesBlocked
             : ThirdPartyCookieBlockState::
                   kThirdPartyCookieBlockingDisabledForSite;
}

void NavigationMetricsRecorder::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  content::NavigationEntry* last_committed_entry =
      web_contents()->GetController().GetLastCommittedEntry();

  const GURL url = last_committed_entry->GetVirtualURL();
  Profile* profile = Profile::FromBrowserContext(context);
  navigation_metrics::RecordPrimaryMainFrameNavigation(
      url, navigation_handle->IsSameDocument(), profile->IsOffTheRecord(),
      profile_metrics::GetBrowserProfileType(context));
  profile->RecordPrimaryMainFrameNavigation();

  if (url.SchemeIsHTTPOrHTTPS() && !navigation_handle->IsSameDocument() &&
      !navigation_handle->IsDownload() && !profile->IsOffTheRecord()) {
    blink::mojom::EngagementLevel engagement_level =
        site_engagement_service_->GetEngagementLevel(url);
    base::UmaHistogramEnumeration("Navigation.MainFrame.SiteEngagementLevel",
                                  engagement_level);
  }
  if (url.SchemeIsHTTPOrHTTPS() && !navigation_handle->IsDownload()) {
    ThirdPartyCookieBlockState block_state = GetThirdPartyCookieBlockState(url);
    base::UmaHistogramEnumeration(
        "Navigation.MainFrame.ThirdPartyCookieBlockingEnabled", block_state);

    if (!base::FeatureList::IsEnabled(
            features::kCookieDeprecationFacilitatedTesting)) {
      return;
    }

    bool is_client_state_eligible = false;
    if (auto* experiment_manager =
            tpcd::experiment::ExperimentManagerImpl::GetForProfile(profile)) {
      is_client_state_eligible = experiment_manager->IsClientEligible() == true;
    }

    bool is_blocked_by_experiment =
        is_client_state_eligible && tpcd::experiment::kDisable3PCookies.Get();
    bool is_block_state_allowed =
        block_state == ThirdPartyCookieBlockState::kCookiesAllowed;

    tpcd::experiment::utils::Experiment3PCBlockStatus experiment_block_status;

    if (is_block_state_allowed && is_blocked_by_experiment) {
      experiment_block_status = tpcd::experiment::utils::
          Experiment3PCBlockStatus::kAllowedAndExperimentBlocked;
    } else if (is_block_state_allowed && !is_blocked_by_experiment) {
      experiment_block_status = tpcd::experiment::utils::
          Experiment3PCBlockStatus::kAllowedAndExperimentAllowed;
    } else if (!is_block_state_allowed && is_blocked_by_experiment) {
      experiment_block_status = tpcd::experiment::utils::
          Experiment3PCBlockStatus::kBlockedAndExperimentBlocked;
    } else {
      experiment_block_status = tpcd::experiment::utils::
          Experiment3PCBlockStatus::kBlockedAndExperimentAllowed;
    }
    base::UmaHistogramEnumeration(
        tpcd::experiment::utils::Experiment3pcBlockStatusHistogramName,
        experiment_block_status);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NavigationMetricsRecorder);
