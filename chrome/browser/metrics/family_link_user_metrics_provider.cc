// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/family_link_user_log_record.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#endif

FamilyLinkUserMetricsProvider::~FamilyLinkUserMetricsProvider() = default;

bool FamilyLinkUserMetricsProvider::ProvideHistograms() {
  // This function is called at unpredictable intervals throughout the Chrome
  // session, so guarantee it will never crash.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profile_list = profile_manager->GetLoadedProfiles();
  std::vector<supervised_user::FamilyLinkUserLogRecord> records;
  for (Profile* profile : profile_list) {
#if !BUILDFLAG(IS_ANDROID)
    // TODO(b/274889379): Mock call to GetBrowserCount().
    if (!FamilyLinkUserMetricsProvider::
            skip_active_browser_count_for_unittesting_ &&
        chrome::GetBrowserCount(profile) == 0) {
      // The profile is loaded, but there's no opened browser for this
      // profile.
      continue;
    }
#endif

    supervised_user::SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile);

    records.push_back(supervised_user::FamilyLinkUserLogRecord::Create(
        IdentityManagerFactory::GetForProfile(profile), *profile->GetPrefs(),
        *HostContentSettingsMapFactory::GetForProfile(profile),
        service ? service->GetURLFilter() : nullptr));
  }
  return supervised_user::EmitLogRecordHistograms(records);
}
