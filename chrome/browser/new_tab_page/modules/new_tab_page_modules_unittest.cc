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
#include "chrome/browser/new_tab_page/modules/modules_constants.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/modules/test_support.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  auto sync_service = std::make_unique<syncer::TestSyncService>();
  sync_service->SetLocalSyncEnabled(true);
  return sync_service;
}

std::unique_ptr<TestingProfile> MakeTestingProfile(
    network::TestURLLoaderFactory& url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetSharedURLLoaderFactory(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &url_loader_factory));
  profile_builder.AddTestingFactory(
      SyncServiceFactory::GetInstance(),
      base::BindRepeating(&CreateTestSyncService));
  profile_builder.AddTestingFactory(
      ChromeSigninClientFactory::GetInstance(),
      base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                          &url_loader_factory));
  return IdentityTestEnvironmentProfileAdaptor::
      CreateProfileForIdentityTestEnvironment(profile_builder);
}

const char kSampleUserEmail[] = "user@gmail.com";
const std::vector<ntp::ModuleIdDetail> kSampleModules = {
    {ntp_modules::kDriveModuleId, IDS_NTP_MODULES_DRIVE_NAME}};

}  // namespace

class NewTabPageModulesTest : public testing::Test {
 public:
  NewTabPageModulesTest() {
    profile_ = MakeTestingProfile(test_url_loader_factory_);
    identity_test_environment_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    identity_test_env().SetTestURLLoaderFactory(&test_url_loader_factory_);
  }

  TestingProfile& profile() { return *profile_; }
  signin::IdentityTestEnvironment& identity_test_env() {
    return *identity_test_environment_profile_adaptor_->identity_test_env();
  }

 private:
  // Must be on Chrome_UIThread.
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_profile_adaptor_;
};

TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_PopulatesStructCorrectly) {
  base::test::ScopedFeatureList features;
  profile().GetTestingPrefService()->SetManagedPref(
      prefs::kNtpSharepointModuleVisible, base::Value(true));
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpMicrosoftAuthenticationModule,
                            ntp_features::kNtpSharepointModule,
                            ntp_features::kNtpMostRelevantTabResumptionModule},
      /*disabled_features=*/{});
  identity_test_env().SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});

  const std::vector<ntp::ModuleIdDetail> module_id_details =
      ntp::MakeModuleIdDetails(/*is_managed_profile=*/false,
                               /*profile=*/&profile());
  EXPECT_EQ(3u, module_id_details.size());
  const auto& microsoft_files_details = module_id_details[0];
  EXPECT_EQ(ntp_modules::kMicrosoftFilesModuleId, microsoft_files_details.id_);
  EXPECT_EQ(IDS_NTP_MODULES_MICROSOFT_FILES_NAME,
            microsoft_files_details.name_message_id_);
  EXPECT_EQ(std::nullopt, microsoft_files_details.description_message_id_);
  const auto& microsoft_auth_details = module_id_details[1];
  EXPECT_EQ(ntp_modules::kMicrosoftAuthenticationModuleId,
            microsoft_auth_details.id_);
  EXPECT_EQ(IDS_NTP_MODULES_MICROSOFT_AUTHENTICATION_NAME,
            microsoft_auth_details.name_message_id_);
  EXPECT_EQ(IDS_NTP_MICROSOFT_AUTHENTICATION_SIDE_PANEL_DESCRIPTION,
            microsoft_auth_details.description_message_id_);
  const auto& tab_resumption_details = module_id_details[2];
  EXPECT_EQ(ntp_modules::kMostRelevantTabResumptionModuleId,
            tab_resumption_details.id_);
  EXPECT_EQ(IDS_NTP_MODULES_MOST_RELEVANT_TAB_RESUMPTION_TITLE,
            tab_resumption_details.name_message_id_);
  EXPECT_EQ(std::nullopt, tab_resumption_details.description_message_id_);
}

TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_OnlyPopulatesEnabledModules) {
  identity_test_env().SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  const std::vector<base::test::FeatureRef>& some_module_features = {
      ntp_features::kNtpFeedModule,
      ntp_features::kNtpMostRelevantTabResumptionModule};
  for (auto& feature : some_module_features) {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{feature},
        /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
            some_module_features, {feature}));

    const std::vector<ntp::ModuleIdDetail> module_id_details =
        ntp::MakeModuleIdDetails(/*is_managed_profile=*/false,
                                 /*profile=*/&profile());
    ASSERT_EQ(1u, module_id_details.size());
  }
}

TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_WithDriveModule) {
  base::test::ScopedFeatureList features;
  const std::vector<base::test::FeatureRef>& enabled_features = {
      ntp_features::kNtpDriveModule};
  features.InitWithFeatures(
      /*enabled_features=*/enabled_features,
      /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
          ntp::kAllModuleFeatures, enabled_features));
  identity_test_env().SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});

  const std::vector<ntp::ModuleIdDetail> module_id_details =
      ntp::MakeModuleIdDetails(/*is_managed_profile=*/true,
                               /*profile=*/&profile());
  ASSERT_EQ(1u, module_id_details.size());
}

TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_Managed) {
  base::test::ScopedFeatureList features;
  const std::vector<base::test::FeatureRef>& enabled_features = {
      ntp_features::kNtpCalendarModule};
  features.InitWithFeatures(
      /*enabled_features=*/enabled_features,
      /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
          ntp::kAllModuleFeatures, enabled_features));
  identity_test_env().SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});

  const std::vector<ntp::ModuleIdDetail> module_id_details =
      ntp::MakeModuleIdDetails(/*is_managed_profile=*/true,
                               /*profile=*/&profile());
  ASSERT_EQ(1u, module_id_details.size());
}

TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_NotManaged) {
  base::test::ScopedFeatureList features;
  const std::vector<base::test::FeatureRef>& enabled_features = {
      ntp_features::kNtpCalendarModule};
  features.InitWithFeatures(
      /*enabled_features=*/enabled_features,
      /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
          ntp::kAllModuleFeatures, enabled_features));

  const std::vector<ntp::ModuleIdDetail> module_id_details =
      ntp::MakeModuleIdDetails(/*is_managed_profile=*/false,
                               /*profile=*/&profile());
  ASSERT_EQ(0u, module_id_details.size());
}

#if !defined(OFFICIAL_BUILD)
TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_DummyModules) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpDummyModules},
      /*disabled_features=*/ntp::kAllModuleFeatures);

  const std::vector<ntp::ModuleIdDetail> module_id_details =
      ntp::MakeModuleIdDetails(/*is_managed_profile=*/false,
                               /*profile=*/&profile());
  ASSERT_EQ(1u, module_id_details.size());
}
#endif

TEST_F(NewTabPageModulesTest, HasModulesEnabled) {
  identity_test_env().SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  ASSERT_TRUE(ntp::HasModulesEnabled(kSampleModules,
                                     identity_test_env().identity_manager()));
}

TEST_F(NewTabPageModulesTest, HasModulesEnabled_NtpModulesLoadFlag) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpModulesLoad},
      /*disabled_features=*/{});

  identity_test_env().SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  ASSERT_FALSE(ntp::HasModulesEnabled(kSampleModules,
                                      identity_test_env().identity_manager()));
}

TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_MicrosoftCards) {
  profile().GetTestingPrefService()->SetManagedPref(
      prefs::kNtpSharepointModuleVisible, base::Value(true));
  profile().GetTestingPrefService()->SetManagedPref(
      prefs::kNtpOutlookModuleVisible, base::Value(true));
  base::test::ScopedFeatureList features;
  const std::vector<base::test::FeatureRef>& enabled_features = {
      ntp_features::kNtpMicrosoftAuthenticationModule,
      ntp_features::kNtpSharepointModule,
      ntp_features::kNtpOutlookCalendarModule,
  };
  features.InitWithFeatures(
      /*enabled_features=*/enabled_features,
      /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
          ntp::kAllModuleFeatures, enabled_features));

  const std::vector<ntp::ModuleIdDetail> module_id_details =
      ntp::MakeModuleIdDetails(/*is_managed_profile=*/true,
                               /*profile=*/&profile());
  ASSERT_EQ(3u, module_id_details.size());
}

TEST_F(NewTabPageModulesTest,
       MakeModuleIdDetails_MicrosoftCardsSharepointOnly) {
  profile().GetTestingPrefService()->SetManagedPref(
      prefs::kNtpSharepointModuleVisible, base::Value(true));
  base::test::ScopedFeatureList features;
  const std::vector<base::test::FeatureRef>& enabled_features = {
      ntp_features::kNtpMicrosoftAuthenticationModule,
      ntp_features::kNtpSharepointModule,
      ntp_features::kNtpOutlookCalendarModule,
  };
  features.InitWithFeatures(
      /*enabled_features=*/enabled_features,
      /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
          ntp::kAllModuleFeatures, enabled_features));

  const std::vector<ntp::ModuleIdDetail> module_id_details =
      ntp::MakeModuleIdDetails(/*is_managed_profile=*/true,
                               /*profile=*/&profile());
  ASSERT_EQ(2u, module_id_details.size());
}

TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_MicrosoftCardsOutlookOnly) {
  profile().GetTestingPrefService()->SetManagedPref(
      prefs::kNtpOutlookModuleVisible, base::Value(true));
  base::test::ScopedFeatureList features;
  const std::vector<base::test::FeatureRef>& enabled_features = {
      ntp_features::kNtpMicrosoftAuthenticationModule,
      ntp_features::kNtpSharepointModule,
      ntp_features::kNtpOutlookCalendarModule,
  };
  features.InitWithFeatures(
      /*enabled_features=*/enabled_features,
      /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
          ntp::kAllModuleFeatures, enabled_features));

  const std::vector<ntp::ModuleIdDetail> module_id_details =
      ntp::MakeModuleIdDetails(/*is_managed_profile=*/true,
                               /*profile=*/&profile());
  ASSERT_EQ(2u, module_id_details.size());
}

TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_MicrosoftCardsDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
          ntp::kAllModuleFeatures, {}));

  const std::vector<ntp::ModuleIdDetail> module_id_details =
      ntp::MakeModuleIdDetails(/*is_managed_profile=*/true,
                               /*profile=*/&profile());
  ASSERT_EQ(0u, module_id_details.size());
}
