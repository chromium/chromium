// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/browser/new_tab_page/modules/test_support.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    bool has_sync_enabled,
    content::BrowserContext* context) {
  auto sync_service = std::make_unique<syncer::TestSyncService>();
  sync_service->SetLocalSyncEnabled(has_sync_enabled);
  return sync_service;
}

std::unique_ptr<TestingProfile> MakeTestingProfile(bool has_sync_enabled) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      SyncServiceFactory::GetInstance(),
      base::BindRepeating(&CreateTestSyncService, has_sync_enabled));
  return profile_builder.Build();
}

}  // namespace

class NewTabPageModulesTest : public testing::Test {
 public:
  NewTabPageModulesTest() : profile_(MakeTestingProfile(false)) {}

  TestingProfile& profile() { return *profile_; }
  void SetSyncEnabled(bool has_sync_enabled) {
    profile_ = MakeTestingProfile(has_sync_enabled);
  }

 private:
  // Must be on Chrome_UIThread.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(NewTabPageModulesTest, MakeModuleIdNames_SingleModuleEnabled) {
  const std::vector<base::test::FeatureRef>& some_module_features = {
      ntp_features::kNtpFeedModule,
      ntp_features::kNtpMostRelevantTabResumptionModule};
  for (auto& feature : some_module_features) {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{feature},
        /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
            some_module_features, {feature}));

    const std::vector<std::pair<const std::string, int>> module_id_names =
        ntp::MakeModuleIdNames(/*is_managed_profile=*/false,
                               /*profile=*/&profile());
    ASSERT_EQ(1u, module_id_names.size());
  }
}

TEST_F(NewTabPageModulesTest, MakeModuleIdNames_WithDriveModule) {
  base::test::ScopedFeatureList features;
  const std::vector<base::test::FeatureRef>& enabled_features = {
      ntp_features::kNtpDriveModule};
  features.InitWithFeatures(
      /*enabled_features=*/enabled_features,
      /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
          ntp::kAllModuleFeatures, enabled_features));
  SetSyncEnabled(true);

  const std::vector<std::pair<const std::string, int>> module_id_names =
      ntp::MakeModuleIdNames(/*is_managed_profile=*/true,
                             /*profile=*/&profile());
  ASSERT_EQ(1u, module_id_names.size());
}

TEST_F(NewTabPageModulesTest, MakeModuleIdNames_Managed) {
  base::test::ScopedFeatureList features;
  const std::vector<base::test::FeatureRef>& enabled_features = {
      ntp_features::kNtpCalendarModule,
      ntp_features::kNtpOutlookCalendarModule,
  };
  features.InitWithFeatures(
      /*enabled_features=*/enabled_features,
      /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
          ntp::kAllModuleFeatures, enabled_features));

  const std::vector<std::pair<const std::string, int>> module_id_names =
      ntp::MakeModuleIdNames(/*is_managed_profile=*/true,
                             /*profile=*/&profile());
  ASSERT_EQ(2u, module_id_names.size());
}

TEST_F(NewTabPageModulesTest, MakeModuleIdNames_NotManaged) {
  base::test::ScopedFeatureList features;
  const std::vector<base::test::FeatureRef>& enabled_features = {
      ntp_features::kNtpCalendarModule,
      ntp_features::kNtpOutlookCalendarModule,
  };
  features.InitWithFeatures(
      /*enabled_features=*/enabled_features,
      /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
          ntp::kAllModuleFeatures, enabled_features));

  const std::vector<std::pair<const std::string, int>> module_id_names =
      ntp::MakeModuleIdNames(/*is_managed_profile=*/false,
                             /*profile=*/&profile());
  ASSERT_EQ(0u, module_id_names.size());
}

#if !defined(OFFICIAL_BUILD)
TEST_F(NewTabPageModulesTest, MakeModuleIdNames_DummyModules) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpDummyModules},
      /*disabled_features=*/ntp::kAllModuleFeatures);

  const std::vector<std::pair<const std::string, int>> module_id_names =
      ntp::MakeModuleIdNames(/*is_managed_profile=*/false,
                             /*profile=*/&profile());
  ASSERT_EQ(1u, module_id_names.size());
}
#endif

const char kSampleUserEmail[] = "user@gmail.com";
const std::vector<std::pair<const std::string, int>> kSampleModules = {
    {"drive", IDS_NTP_MODULES_DRIVE_SENTENCE}};

TEST_F(NewTabPageModulesTest, HasModulesEnabled_SignedInAccount) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  ASSERT_TRUE(ntp::HasModulesEnabled(kSampleModules,
                                     identity_test_env.identity_manager()));
}

TEST_F(NewTabPageModulesTest,
       HasModulesEnabled_SignedInAccountNtpModulesLoadFlag) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpModulesLoad},
      /*disabled_features=*/{});

  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  ASSERT_FALSE(ntp::HasModulesEnabled(kSampleModules,
                                      identity_test_env.identity_manager()));
}

TEST_F(NewTabPageModulesTest, HasModulesEnabled_NoSignedInAccount) {
  signin::IdentityTestEnvironment identity_test_env;
  ASSERT_FALSE(ntp::HasModulesEnabled(kSampleModules,
                                      identity_test_env.identity_manager()));
}

TEST_F(NewTabPageModulesTest,
       HasModulesEnabled_NoSignedInAccountSignedOutModulesSwitch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSignedOutNtpModulesSwitch);
  signin::IdentityTestEnvironment identity_test_env;
  ASSERT_TRUE(ntp::HasModulesEnabled(kSampleModules,
                                     identity_test_env.identity_manager()));
}
