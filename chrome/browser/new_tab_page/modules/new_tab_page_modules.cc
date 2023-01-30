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
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"

namespace ntp {

const std::vector<std::pair<const std::string, int>> MakeModuleIdNames(
    bool drive_module_enabled) {
  std::vector<std::pair<const std::string, int>> details;

  if (IsRecipeTasksModuleEnabled()) {
    std::vector<std::string> splitExperimentGroup = base::SplitString(
        base::GetFieldTrialParamValueByFeature(
            ntp_features::kNtpRecipeTasksModule,
            ntp_features::kNtpRecipeTasksModuleExperimentGroupParam),
        "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    bool recipes_historical_experiment_enabled =
        !splitExperimentGroup.empty() &&
        splitExperimentGroup[0] == "historical";

    details.emplace_back("recipe_tasks",
                         recipes_historical_experiment_enabled
                             ? IDS_NTP_MODULES_RECIPE_VIEWED_TASKS_SENTENCE
                             : IDS_NTP_MODULES_RECIPE_TASKS_SENTENCE);
  }

  if (IsCartModuleEnabled()) {
    details.emplace_back("chrome_cart", IDS_NTP_MODULES_CART_SENTENCE);
  }

  if (drive_module_enabled) {
    details.emplace_back("drive", IDS_NTP_MODULES_DRIVE_SENTENCE);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpPhotosModule)) {
    details.emplace_back("photos", IDS_NTP_MODULES_PHOTOS_MEMORIES_TITLE);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpFeedModule)) {
    details.emplace_back("feed", IDS_NTP_MODULES_FEED_TITLE);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpHistoryClustersModule)) {
    details.emplace_back("history-clusters",
                         IDS_HISTORY_CLUSTERS_JOURNEYS_TAB_LABEL);
  }

#if !defined(OFFICIAL_BUILD)
  if (base::FeatureList::IsEnabled(ntp_features::kNtpDummyModules)) {
    details.emplace_back("dummy", IDS_NTP_MODULES_DUMMY_TITLE);

    for (int i = 2; i <= 12; i++) {
      details.emplace_back(base::StringPrintf("dummy%d", i),
                           IDS_NTP_MODULES_DUMMY2_TITLE);
    }
  }
#endif

  return details;
}

bool HasModulesEnabled(
    std::vector<std::pair<const std::string, int>> module_id_names,
    signin::IdentityManager* identity_manager) {
  return !module_id_names.empty() &&
         !base::FeatureList::IsEnabled(ntp_features::kNtpModulesLoad) &&
         (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kSignedOutNtpModulesSwitch) ||
          (/* Can be null if Chrome signin is disabled. */ identity_manager &&
           identity_manager->GetAccountsInCookieJar()
                   .signed_in_accounts.size() > 0));
}

}  // namespace ntp
