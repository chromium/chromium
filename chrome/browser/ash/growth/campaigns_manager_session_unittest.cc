// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_session.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/growth/campaigns_manager_client_impl.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
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
    InitializeComponentManager();
    session_manager_ = std::make_unique<session_manager::SessionManager>();
  }

  void TearDown() override {
    ash::ConciergeClient::Shutdown();

    component_manager_ash_ = nullptr;
    owner_settings_service_ash_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownComponentManager();
    profile_manager_->DeleteAllTestingProfiles();
  }

 protected:
  bool FinishCampaignsComponentLoad(const base::FilePath& mount_path) {
    EXPECT_TRUE(component_manager_ash_->HasPendingInstall(kCampaignsComponent));
    EXPECT_TRUE(component_manager_ash_->UpdateRequested(kCampaignsComponent));

    return component_manager_ash_->FinishLoadRequest(
        kCampaignsComponent,
        component_updater::FakeComponentManagerAsh::ComponentInfo(
            component_updater::ComponentManagerAsh::Error::NONE,
            base::FilePath("/dev/null"), mount_path));
  }

  void InitializeComponentManager() {
    auto fake_component_manager_ash =
        base::MakeRefCounted<component_updater::FakeComponentManagerAsh>();
    fake_component_manager_ash->set_queue_load_requests(true);
    fake_component_manager_ash->set_supported_components({kCampaignsComponent});
    component_manager_ash_ = fake_component_manager_ash.get();

    browser_process_platform_part_test_api_.InitializeComponentManager(
        std::move(fake_component_manager_ash));
  }

  void FlushActiveProfileCallbacks(bool is_owner) {
    DCHECK(owner_settings_service_ash_);
    owner_settings_service_ash_->RunPendingIsOwnerCallbacksForTesting(is_owner);
  }

  // Creates a test user with a testing profile and logs in.
  TestingProfile* LoginUser() {
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("test@test.com", "test_user"));
    fake_user_manager_->AddUser(account_id);

    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());

    fake_user_manager_->LoginUser(account_id);

    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        account_id.GetUserEmail(),
        {TestingProfile::TestingFactory{
            ash::OwnerSettingsServiceAshFactory::GetInstance(),
            base::BindRepeating(
                &CampaignsManagerSessionTest::CreateOwnerSettingsServiceAsh,
                base::Unretained(this))}});

    owner_settings_service_ash_ =
        ash::OwnerSettingsServiceAshFactory::GetInstance()
            ->GetForBrowserContext(profile);
    return profile;
  }

  raw_ptr<component_updater::FakeComponentManagerAsh> component_manager_ash_ =
      nullptr;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  std::unique_ptr<KeyedService> CreateOwnerSettingsServiceAsh(
      content::BrowserContext* context) {
    return scoped_cros_settings_test_helper_.CreateOwnerSettingsService(
        Profile::FromBrowserContext(context));
  }

  BrowserProcessPlatformPartTestApi browser_process_platform_part_test_api_;
  ash::ScopedCrosSettingsTestHelper scoped_cros_settings_test_helper_;
  raw_ptr<ash::OwnerSettingsServiceAsh> owner_settings_service_ash_;
  CampaignsManagerClientImpl client_;
};

TEST_F(CampaignsManagerSessionTest, LoadCampaignsComponent) {
  LoginUser();
  auto campaigns_manager_session = CampaignsManagerSession();
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  FlushActiveProfileCallbacks(/*is_owner=*/false);

  ASSERT_TRUE(
      FinishCampaignsComponentLoad(base::FilePath(kCampaignsMountPoint)));
  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kCampaignsComponent));
}

TEST_F(CampaignsManagerSessionTest, LoadCampaignsComponentLoggedInNotActive) {
  auto campaigns_manager_session = CampaignsManagerSession();
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);

  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kCampaignsComponent));
}

TEST_F(CampaignsManagerSessionTest, LoadCampaignsComponentManagedDevice) {
  auto campaigns_manager_session = CampaignsManagerSession();
  TestingProfile::Builder builder;
  builder.OverridePolicyConnectorIsManagedForTesting(/*is_managed=*/true);
  auto profile = builder.Build();
  campaigns_manager_session.SetProfileForTesting(profile.get());
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);

  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kCampaignsComponent));
}

TEST_F(CampaignsManagerSessionTest, LoadCampaignsComponentGuestMode) {
  auto campaigns_manager_session = CampaignsManagerSession();
  auto* profile = profile_manager_->CreateGuestProfile()->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
  campaigns_manager_session.SetProfileForTesting(profile);
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);

  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kCampaignsComponent));
}
