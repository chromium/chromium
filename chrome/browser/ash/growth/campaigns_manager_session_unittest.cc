// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_session.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/growth/campaigns_manager_client_impl.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kCampaignsComponent[] = "growth-campaigns";
constexpr char kCampaignsMountPoint[] = "/run/imageloader/growth_campaigns";

}  // namespace

class CampaignsManagerSessionTest : public testing::Test {
 public:
  CampaignsManagerSessionTest()
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())),
        browser_process_platform_part_test_api_(
            g_browser_process->platform_part()) {}

  CampaignsManagerSessionTest(const CampaignsManagerSessionTest&) = delete;
  CampaignsManagerSessionTest& operator=(const CampaignsManagerSessionTest&) =
      delete;

  ~CampaignsManagerSessionTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_->SetUp());
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    InitializeCrosComponentManager();
    session_manager_ = std::make_unique<session_manager::SessionManager>();
  }

  void TearDown() override {
    ash::ConciergeClient::Shutdown();

    cros_component_manager_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownCrosComponentManager();
    profile_manager_->DeleteAllTestingProfiles();
  }

 protected:
  bool FinishCampaignsComponentLoad(const base::FilePath& mount_path) {
    EXPECT_TRUE(
        cros_component_manager_->HasPendingInstall(kCampaignsComponent));
    EXPECT_TRUE(cros_component_manager_->UpdateRequested(kCampaignsComponent));

    return cros_component_manager_->FinishLoadRequest(
        kCampaignsComponent,
        component_updater::FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/dev/null"), mount_path));
  }

  void InitializeCrosComponentManager() {
    auto fake_cros_component_manager =
        base::MakeRefCounted<component_updater::FakeCrOSComponentManager>();
    fake_cros_component_manager->set_queue_load_requests(true);
    fake_cros_component_manager->set_supported_components(
        {kCampaignsComponent});
    cros_component_manager_ = fake_cros_component_manager.get();

    browser_process_platform_part_test_api_.InitializeCrosComponentManager(
        std::move(fake_cros_component_manager));
  }

  // Creates a test user with a testing profile and logs in.
  TestingProfile* LoginUser() {
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("test@test.com", "test_user"));
    fake_user_manager_->AddUser(account_id);

    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        account_id.GetUserEmail(), std::move(prefs), u"Test profile",
        /*avatar_id=*/1, TestingProfile::TestingFactories());

    fake_user_manager_->LoginUser(account_id);
    return profile;
  }

  raw_ptr<component_updater::FakeCrOSComponentManager> cros_component_manager_ =
      nullptr;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  BrowserProcessPlatformPartTestApi browser_process_platform_part_test_api_;
  CampaignsManagerClientImpl client_;
};

TEST_F(CampaignsManagerSessionTest, LoadCampaignsComponent) {
  LoginUser();
  auto campaigns_manager_session = CampaignsManagerSession();
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);

  ASSERT_TRUE(
      FinishCampaignsComponentLoad(base::FilePath(kCampaignsMountPoint)));
  EXPECT_FALSE(cros_component_manager_->HasPendingInstall(kCampaignsComponent));
}

TEST_F(CampaignsManagerSessionTest, LoadCampaignsComponentLoggedInNotActive) {
  auto campaigns_manager_session = CampaignsManagerSession();
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);

  EXPECT_FALSE(cros_component_manager_->HasPendingInstall(kCampaignsComponent));
}

TEST_F(CampaignsManagerSessionTest, LoadCampaignsComponentManagedDevice) {
  auto campaigns_manager_session = CampaignsManagerSession();
  TestingProfile::Builder builder;
  builder.OverridePolicyConnectorIsManagedForTesting(/*is_managed=*/true);
  auto profile = builder.Build();
  campaigns_manager_session.SetProfileForTesting(profile.get());
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);

  EXPECT_FALSE(cros_component_manager_->HasPendingInstall(kCampaignsComponent));
}
