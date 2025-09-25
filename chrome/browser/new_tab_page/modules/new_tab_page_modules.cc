// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/new_tab_page/modules/modules_constants.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/history_clusters/core/features.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"

namespace ntp {

const std::vector<ModuleIdDetail> MakeModuleIdDetails(bool is_managed_profile,
                                                      Profile* profile) {
  std::vector<ModuleIdDetail> details;

  if (IsGoogleCalendarModuleEnabled(is_managed_profile, profile)) {
    details.emplace_back(ntp_modules::kGoogleCalendarModuleId,
                         IDS_NTP_MODULES_GOOGLE_CALENDAR_TITLE);
  }

  if (IsOutlookCalendarModuleEnabledForProfile(profile)) {
    details.emplace_back(ntp_modules::kOutlookCalendarModuleId,
                         IDS_NTP_MODULES_OUTLOOK_CALENDAR_TITLE);
  }

  if (IsDriveModuleEnabledForProfile(is_managed_profile, profile)) {
    details.emplace_back(ntp_modules::kDriveModuleId,
                         IDS_NTP_MODULES_DRIVE_NAME);
  }

  if (IsMicrosoftFilesModuleEnabledForProfile(profile)) {
    details.emplace_back(ntp_modules::kMicrosoftFilesModuleId,
                         IDS_NTP_MODULES_MICROSOFT_FILES_NAME);
  }

  if (IsMicrosoftModuleEnabledForProfile(profile)) {
    details.emplace_back(
        ntp_modules::kMicrosoftAuthenticationModuleId,
        IDS_NTP_MODULES_MICROSOFT_AUTHENTICATION_NAME,
        IDS_NTP_MICROSOFT_AUTHENTICATION_SIDE_PANEL_DESCRIPTION);
  }

  if (IsMostRelevantTabResumeModuleEnabled(profile)) {
    details.emplace_back(ntp_modules::kMostRelevantTabResumptionModuleId,
                         IDS_NTP_MODULES_MOST_RELEVANT_TAB_RESUMPTION_TITLE);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpFeedModule)) {
    details.emplace_back(ntp_modules::kFeedModuleId,
                         IDS_NTP_MODULES_FEED_TITLE);
  }

#if !defined(OFFICIAL_BUILD)
  if (base::FeatureList::IsEnabled(ntp_features::kNtpDummyModules)) {
    details.emplace_back(ntp_modules::kDummyModuleId,
                         IDS_NTP_MODULES_DUMMY_TITLE);
  }
#endif

  if (base::FeatureList::IsEnabled(ntp_features::kNtpTabGroupsModule)) {
    details.emplace_back(ntp_modules::kTabGroupsModuleId,
                         IDS_NTP_MODULES_TAB_GROUPS_TITLE);
  }

  return details;
}

bool HasModulesEnabled(const std::vector<ModuleIdDetail> module_id_details,
                       signin::IdentityManager* identity_manager) {
  return !module_id_details.empty() &&
         !base::FeatureList::IsEnabled(ntp_features::kNtpModulesLoad) &&
         (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kSignedOutNtpModulesSwitch) ||
          base::FeatureList::IsEnabled(
              ntp_features::kNtpModuleSignInRequirement) ||
          (/* Can be null if Chrome signin is disabled. */ identity_manager &&
           identity_manager->GetAccountsInCookieJar()
                   .GetPotentiallyInvalidSignedInAccounts()
                   .size() > 0));
}

}  // namespace ntp
