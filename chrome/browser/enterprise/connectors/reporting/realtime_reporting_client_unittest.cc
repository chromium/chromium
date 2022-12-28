// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/enterprise/browser/enterprise_switches.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/scoped_test_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#else
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#endif

using ::testing::Mock;

namespace enterprise_connectors {

std::unique_ptr<KeyedService> BuildRealtimeReportingClient(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(new RealtimeReportingClient(context));
}

class RealtimeReportingClientTestBase : public testing::Test {
 public:
  RealtimeReportingClientTestBase()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
  }

  RealtimeReportingClientTestBase(const RealtimeReportingClientTestBase&) =
      delete;
  RealtimeReportingClientTestBase& operator=(
      const RealtimeReportingClientTestBase&) = delete;

  ~RealtimeReportingClientTestBase() override = default;

  void SetUp() override {
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("fake-token"));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<extensions::TestEventRouter> event_router_ = nullptr;

 private:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FakeBrowserDMTokenStorage dm_token_storage_;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
};

// Tests to make sure the feature flag and policy control real-time reporting
// as expected.  The parameter for these tests is a tuple of bools:
//
//   bool: whether the feature flag is enabled.
//   bool: whether the session is public or not.
class RealtimeReportingClientIsRealtimeReportingEnabledTest
    : public RealtimeReportingClientTestBase,
      public testing::WithParamInterface<testing::tuple<bool, bool>> {
 public:
  RealtimeReportingClientIsRealtimeReportingEnabledTest()
      : is_feature_flag_enabled_(testing::get<0>(GetParam())),
        is_public_session_(testing::get<1>(GetParam())) {
    if (is_feature_flag_enabled_) {
      scoped_feature_list_.InitWithFeatures(
          {enterprise_connectors::kEnterpriseConnectorsEnabledOnMGS}, {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {enterprise_connectors::kEnterpriseConnectorsEnabledOnMGS});
    }

    // In chrome branded desktop builds, the browser is always manageable.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableChromeBrowserCloudManagement);
#endif
  }

  void SetUp() override {
    RealtimeReportingClientTestBase::SetUp();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    const AccountId account_id(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));
    const user_manager::User* user;
    if (is_public_session_) {
      user = user_manager->AddPublicAccountUser(account_id);
    } else {
      user = user_manager->AddUserWithAffiliation(account_id,
                                                  /*is_affiliated=*/true);
    }
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                 profile_);
    user_manager->UserLoggedIn(account_id, user->username_hash(),
                               /*browser_restart=*/false,
                               /*is_child=*/false);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    profile_->ScopedCrosSettingsTestHelper()
        ->InstallAttributes()
        ->SetCloudManaged("domain.com", "device_id");
#endif
  }

  bool should_init() {
    return is_feature_flag_enabled_ || !profiles::IsPublicSession();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  const bool is_feature_flag_enabled_;
  const bool is_public_session_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
 private:
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif
};

TEST_P(RealtimeReportingClientIsRealtimeReportingEnabledTest,
       ShouldInitRealtimeReportingClient) {
  EXPECT_EQ(should_init(), enterprise_connectors::RealtimeReportingClient::
                               ShouldInitRealtimeReportingClient());
}

INSTANTIATE_TEST_SUITE_P(All,
                         RealtimeReportingClientIsRealtimeReportingEnabledTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace enterprise_connectors
