// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_session.h"

#include <memory>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/growth/campaigns_manager_test_helper.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/account_id/account_id.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kCampaignsComponent[] = "growth-campaigns";
constexpr char kCampaignsMountPoint[] = "/run/imageloader/growth_campaigns";

}  // namespace

class CampaignsManagerSessionTest : public testing::Test {
 public:
  CampaignsManagerSessionTest() {
    auto delegate =
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            TestingBrowserProcess::GetGlobal());
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(
            TestingBrowserProcess::GetGlobal()->local_state(),
            std::move(delegate));
  }

  CampaignsManagerSessionTest(const CampaignsManagerSessionTest&) = delete;
  CampaignsManagerSessionTest& operator=(const CampaignsManagerSessionTest&) =
      delete;

  ~CampaignsManagerSessionTest() override = default;

  void SetUp() override {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    component_manager_ash_ =
        base::MakeRefCounted<component_updater::FakeComponentManagerAsh>();
    component_manager_ash_->set_queue_load_requests(true);
    component_manager_ash_->set_supported_components({kCampaignsComponent});
    test_helper_.InitializeCampaignsManager(component_manager_ash_);
  }

  void TearDown() override {
    owner_settings_service_ash_ = nullptr;

    test_helper_.ShutdownCampaignsManager();
    component_manager_ash_ = nullptr;

    ash::ConciergeClient::Shutdown();
    user_session_test_environment_.reset();
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

  void FlushActiveProfileCallbacks(bool is_owner) {
    DCHECK(owner_settings_service_ash_);
    owner_settings_service_ash_->RunPendingIsOwnerCallbacksForTesting(is_owner);
  }

  // Logs in a real user session and returns its profile, so the session
  // resolves the profile instead of an injected one.
  TestingProfile* LoginRegularUser() {
    // Install the fake owner settings service before the profile is created.
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&CampaignsManagerSessionTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));

    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("test@test.com", GaiaId("test_user")));
    CHECK(user_session_test_environment_->AddRegularUser(account_id));
    user_session_test_environment_->LogIn(account_id);

    auto* profile = static_cast<TestingProfile*>(Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            account_id)));
    owner_settings_service_ash_ =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(profile);
    return profile;
  }

  scoped_refptr<component_updater::FakeComponentManagerAsh>
      component_manager_ash_ = nullptr;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ash::OwnerSettingsServiceAshFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            &CampaignsManagerSessionTest::CreateOwnerSettingsServiceAsh,
            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateOwnerSettingsServiceAsh(
      content::BrowserContext* context) {
    return scoped_cros_settings_test_helper_.CreateOwnerSettingsService(
        Profile::FromBrowserContext(context));
  }

  base::CallbackListSubscription create_services_subscription_;
  ash::ScopedCrosSettingsTestHelper scoped_cros_settings_test_helper_;
  raw_ptr<ash::OwnerSettingsServiceAsh> owner_settings_service_ash_ = nullptr;

  CampaignsManagerTestHelper test_helper_;
};

TEST_F(CampaignsManagerSessionTest, LoadCampaignsComponent) {
  TestingProfile* profile = LoginRegularUser();
  ASSERT_TRUE(profile);
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Construct after the session is active so it reads the profile from the
  // session.
  auto campaigns_manager_session = CampaignsManagerSession();
  FlushActiveProfileCallbacks(/*is_owner=*/false);

  ASSERT_TRUE(
      FinishCampaignsComponentLoad(base::FilePath(kCampaignsMountPoint)));
  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kCampaignsComponent));
}

TEST_F(CampaignsManagerSessionTest, LoadCampaignsComponentLoggedInNotActive) {
  auto campaigns_manager_session = CampaignsManagerSession();
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);

  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kCampaignsComponent));
}

TEST_F(CampaignsManagerSessionTest, LoadCampaignsComponentManagedDevice) {
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      "managed@test.com", GaiaId("managed_user")));
  ASSERT_TRUE(user_session_test_environment_->AddRegularUser(account_id));
  user_manager::UserManager::Get()->SetUserPolicyStatus(
      account_id, /*is_managed=*/true, /*is_affiliated=*/true);
  user_session_test_environment_->LogIn(account_id);

  auto campaigns_manager_session = CampaignsManagerSession();
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kCampaignsComponent));
}

TEST_F(CampaignsManagerSessionTest, LoadCampaignsComponentGuestMode) {
  user_manager::User* user = user_session_test_environment_->AddGuestUser();
  ASSERT_TRUE(user);
  user_session_test_environment_->LogIn(user->GetAccountId());

  auto campaigns_manager_session = CampaignsManagerSession();
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kCampaignsComponent));
}
