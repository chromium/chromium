// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"

#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "components/navigation_metrics/navigation_metrics.h"
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

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

NavigationMetricsRecorder::NavigationMetricsRecorder(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
#if defined(OS_ANDROID)
  // The site isolation synthetic field trial is only needed on Android, as on
  // desktop it would be unnecessarily set for all users.
  is_synthetic_isolation_trial_enabled_ = true;
#else
  is_synthetic_isolation_trial_enabled_ = false;
#endif
}

NavigationMetricsRecorder::~NavigationMetricsRecorder() {
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
  // to go before the IsInMainFrame() check, as we want to register navigations
  // to isolated sites from both main frames and subframes.
  if (is_synthetic_isolation_trial_enabled_ &&
      navigation_handle->GetRenderFrameHost()
          ->GetSiteInstance()
          ->RequiresDedicatedProcess()) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "SiteIsolationActive", "Enabled");
  }

  // Also register a synthetic field trial when we encounter a navigation to an
  // OOPIF.
  if (is_synthetic_isolation_trial_enabled_ &&
      navigation_handle->GetRenderFrameHost()->IsCrossProcessSubframe()) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "OutOfProcessIframesActive", "Enabled");
  }

  if (!navigation_handle->IsInMainFrame())
    return;

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  content::NavigationEntry* last_committed_entry =
      web_contents()->GetController().GetLastCommittedEntry();

  Profile* profile = Profile::FromBrowserContext(context);
  navigation_metrics::RecordMainFrameNavigation(
      last_committed_entry->GetVirtualURL(),
      navigation_handle->IsSameDocument(), profile->IsOffTheRecord(),
      ProfileMetrics::GetBrowserProfileType(profile));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NavigationMetricsRecorder)
