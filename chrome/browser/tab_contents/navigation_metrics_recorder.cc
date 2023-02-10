// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/navigation_metrics/navigation_metrics.h"
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

#if BUILDFLAG(IS_ANDROID)
  // The site isolation synthetic field trial is only needed on Android, as on
  // desktop it would be unnecessarily set for all users.
  is_synthetic_isolation_trial_enabled_ = true;
#else
  is_synthetic_isolation_trial_enabled_ = false;
#endif
}

NavigationMetricsRecorder::~NavigationMetricsRecorder() = default;

ThirdPartyCookieBlockState
NavigationMetricsRecorder::GetThirdPartyCookieBlockState(const GURL& url) {
  if (!cookie_settings_->ShouldBlockThirdPartyCookies())
    return ThirdPartyCookieBlockState::kCookiesAllowed;
  bool blocking_enabled_for_site =
      !cookie_settings_->IsThirdPartyAccessAllowed(url,
                                                   /*source=*/nullptr);
  return blocking_enabled_for_site
             ? ThirdPartyCookieBlockState::kThirdPartyCookiesBlocked
             : ThirdPartyCookieBlockState::
                   kThirdPartyCookieBlockingDisabledForSite;
}

void NavigationMetricsRecorder::EnableSiteIsolationSyntheticTrialForTesting() {
  is_synthetic_isolation_trial_enabled_ = true;
}

void NavigationMetricsRecorder::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!navigation_handle->HasCommitted())
    return;

  // See if the navigation committed for a site that required a dedicated
  // process and register a synthetic field trial if so.  Note that this needs
  // to go before the IsInPrimaryMainFrame() check, as we want to register
  // navigations to isolated sites from both main frames and subframes.
  auto* site_instance =
      navigation_handle->GetRenderFrameHost()->GetSiteInstance();
  if (is_synthetic_isolation_trial_enabled_ &&
      site_instance->RequiresDedicatedProcess()) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "SiteIsolationActive", "Enabled");
  }

  if (site_instance->RequiresOriginKeyedProcess()) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "ProcessIsolatedOriginAgentClusterActive", "Enabled");
  }

  // Also register a synthetic field trial when we encounter a navigation to an
  // OOPIF.
  if (is_synthetic_isolation_trial_enabled_ &&
      navigation_handle->GetRenderFrameHost()->IsCrossProcessSubframe()) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "OutOfProcessIframesActive", "Enabled");
  }

  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

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

    if (navigation_handle->IsFormSubmission()) {
      base::UmaHistogramEnumeration(
          "Navigation.MainFrameFormSubmission.SiteEngagementLevel",
          engagement_level);
    }
  }
  if (url.SchemeIsHTTPOrHTTPS() && !navigation_handle->IsDownload()) {
    base::UmaHistogramEnumeration(
        "Navigation.MainFrame.ThirdPartyCookieBlockingEnabled",
        GetThirdPartyCookieBlockState(url));
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NavigationMetricsRecorder);
