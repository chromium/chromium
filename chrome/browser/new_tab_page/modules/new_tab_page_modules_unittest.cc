// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/grit/generated_resources.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp {

TEST(NewTabPageModulesTest, MakeModuleIdNames_NoDriveModule) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpRecipeTasksModule},
      /*disabled_features=*/{});

  const std::vector<std::pair<const std::string, int>> module_id_names =
      MakeModuleIdNames(false);
  ASSERT_EQ(1u, module_id_names.size());
}

TEST(NewTabPageModulesTest, MakeModuleIdNames_WithDriveModule) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpRecipeTasksModule,
                            ntp_features::kNtpDriveModule},
      /*disabled_features=*/{});

  const std::vector<std::pair<const std::string, int>> module_id_names =
      MakeModuleIdNames(true);
  ASSERT_EQ(2u, module_id_names.size());
}

#if !defined(OFFICIAL_BUILD)
TEST(NewTabPageModulesTest, MakeModuleIdNames_DummyModules) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpDummyModules},
      /*disabled_features=*/{});

  const std::vector<std::pair<const std::string, int>> module_id_names =
      MakeModuleIdNames(false);
  ASSERT_EQ(12u, module_id_names.size());
}
#endif

const char kSampleUserEmail[] = "user@gmail.com";
const std::vector<std::pair<const std::string, int>> kSampleModules = {
    {"recipe_tasks", IDS_NTP_MODULES_RECIPE_TASKS_SENTENCE}};

TEST(NewTabPageModulesTest, HasModulesEnabled_SignedInAccount) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  ASSERT_TRUE(
      HasModulesEnabled(kSampleModules, identity_test_env.identity_manager()));
}

TEST(NewTabPageModulesTest,
     HasModulesEnabled_SignedInAccountNtpModulesLoadFlag) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpModulesLoad},
      /*disabled_features=*/{});

  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  ASSERT_FALSE(
      HasModulesEnabled(kSampleModules, identity_test_env.identity_manager()));
}

TEST(NewTabPageModulesTest, HasModulesEnabled_NoSignedInAccount) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  ASSERT_FALSE(
      HasModulesEnabled(kSampleModules, identity_test_env.identity_manager()));
}

TEST(NewTabPageModulesTest,
     HasModulesEnabled_NoSignedInAccountSignedOutModulesSwitch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSignedOutNtpModulesSwitch);
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  ASSERT_TRUE(
      HasModulesEnabled(kSampleModules, identity_test_env.identity_manager()));
}

}  // namespace ntp
