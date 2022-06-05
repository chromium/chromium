// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_and_sync_status_metrics_provider.h"

#include <vector>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#endif

#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

ChromeSigninAndSyncStatusMetricsProvider::
    ChromeSigninAndSyncStatusMetricsProvider() = default;
ChromeSigninAndSyncStatusMetricsProvider::
    ~ChromeSigninAndSyncStatusMetricsProvider() = default;

void ChromeSigninAndSyncStatusMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  EmitHistograms(GetStatusOfAllProfiles());
}

signin_metrics::ProfilesStatus
ChromeSigninAndSyncStatusMetricsProvider::GetStatusOfAllProfiles() const {
  signin_metrics::ProfilesStatus profiles_status;

#if !BUILDFLAG(IS_ANDROID)
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    std::vector<Profile*> profile_list = profile_manager->GetLoadedProfiles();
    for (Profile* profile : profile_list) {
        if (chrome::GetBrowserCount(profile) == 0) {
            // The profile is loaded, but there's no opened browser for this profile.
            continue;
        }

        auto* session_duration =
                metrics::DesktopProfileSessionDurationsServiceFactory::
                GetForBrowserContext(profile);
        // |session_duration| will be null for system and guest profiles.
        if (!session_duration) {
            continue;
        }
        UpdateProfilesStatusBasedOnSignInAndSyncStatus(
                profiles_status, session_duration->IsSignedIn(),
                session_duration->IsSyncing());
    }
#endif
  return profiles_status;
}
