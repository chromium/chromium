// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_session.h"

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/wallpaper/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "components/language/core/browser/pref_names.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::component_updater::FakeComponentManagerAsh;

constexpr char kResourcesComponent[] = "demo-mode-resources";
constexpr char kTestDemoModeResourcesMountPoint[] =
    "/run/imageloader/demo_mode_resources";

class DemoSessionTest : public testing::Test {
 public:
  DemoSessionTest()
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())),
        browser_process_platform_part_test_api_(
            g_browser_process->platform_part()) {
    cros_settings_test_helper_.InstallAttributes()->SetDemoMode();
  }

  DemoSessionTest(const DemoSessionTest&) = delete;
  DemoSessionTest& operator=(const DemoSessionTest&) = delete;

  ~DemoSessionTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_->SetUp());
    ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOnline);
    InitializeComponentManager();
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    wallpaper_controller_client_ = std::make_unique<
        WallpaperControllerClientImpl>(
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);
    // TODO(b/321321392): Test loading growth campaigns at session start.
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kGrowthCampaignsInDemoMode);
  }

  void TearDown() override {
    DemoSession::ShutDownIfInitialized();
    DemoSession::ResetDemoConfigForTesting();

    wallpaper_controller_client_.reset();
    ConciergeClient::Shutdown();

    component_manager_ash_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownComponentManager();
    profile_manager_->DeleteAllTestingProfiles();
  }

 protected:
  bool FinishResourcesComponentLoad(const base::FilePath& mount_path) {
    EXPECT_TRUE(component_manager_ash_->HasPendingInstall(kResourcesComponent));
    EXPECT_TRUE(component_manager_ash_->UpdateRequested(kResourcesComponent));

    return component_manager_ash_->FinishLoadRequest(
        kResourcesComponent,
        FakeComponentManagerAsh::ComponentInfo(
            component_updater::ComponentManagerAsh::Error::NONE,
            base::FilePath("/dev/null"), mount_path));
  }

  void InitializeComponentManager() {
    auto fake_component_manager_ash =
        base::MakeRefCounted<FakeComponentManagerAsh>();
    fake_component_manager_ash->set_queue_load_requests(true);
    fake_component_manager_ash->set_supported_components({kResourcesComponent});
    component_manager_ash_ = fake_component_manager_ash.get();

    browser_process_platform_part_test_api_.InitializeComponentManager(
        std::move(fake_component_manager_ash));
  }

  // Creates a dummy demo user with a testing profile and logs in.
  TestingProfile* LoginDemoUser() {
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("demo@test.com", "demo_user"));
    fake_user_manager_->AddPublicAccountUser(account_id);

    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        account_id.GetUserEmail(), std::move(prefs), u"Test profile",
        /*avatar_id=*/1, TestingProfile::TestingFactories());

    fake_user_manager_->LoginUser(account_id);
    return profile;
  }

  raw_ptr<FakeComponentManagerAsh> component_manager_ash_ = nullptr;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
  TestWallpaperController test_wallpaper_controller_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  base::UserActionTester user_action_tester_;

 private:
  BrowserProcessPlatformPartTestApi browser_process_platform_part_test_api_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DemoSessionTest, StartForDeviceInDemoMode) {
  EXPECT_FALSE(DemoSession::Get());
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);
  EXPECT_TRUE(demo_session->started());
  EXPECT_EQ(demo_session, DemoSession::Get());
}

TEST_F(DemoSessionTest, StartForDemoDeviceNotInDemoMode) {
  cros_settings_test_helper_.InstallAttributes()->SetConsumerOwned();
  EXPECT_FALSE(DemoSession::Get());
  EXPECT_FALSE(DemoSession::StartIfInDemoMode());
  EXPECT_FALSE(DemoSession::Get());

  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kResourcesComponent));
}

TEST_F(DemoSessionTest, ShutdownResetsInstance) {
  ASSERT_TRUE(DemoSession::StartIfInDemoMode());
  EXPECT_TRUE(DemoSession::Get());
  DemoSession::ShutDownIfInitialized();
  EXPECT_FALSE(DemoSession::Get());
}

TEST_F(DemoSessionTest, LoginDemoSession) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);
  // There should be no user action DemoMode.DemoSessionStarts reported
  // before the user login
  EXPECT_EQ(0,
            user_action_tester_.GetActionCount("DemoMode.DemoSessionStarts"));

  LoginDemoUser();
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("DemoMode.DemoSessionStarts"));
}

TEST_F(DemoSessionTest, ShowAndRemoveSplashScreen) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  std::unique_ptr<base::MockOneShotTimer> timer =
      std::make_unique<base::MockOneShotTimer>();
  demo_session->SetTimerForTesting(std::move(timer));

  EXPECT_EQ(0, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());

  ASSERT_TRUE(FinishResourcesComponentLoad(
      base::FilePath(kTestDemoModeResourcesMountPoint)));
  // Wait for splash screen image to load and timer to be set
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count(
                   /*always_on_top=*/true));
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());

  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  // The splash screen is not removed upon the active session starts.
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count(
                   /*always_on_top=*/true));
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());

  // Explicitly remove the splash screen as if the fullscreen is toggled.
  demo_session->RemoveSplashScreen();
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count(
                   /*always_on_top=*/true));
  EXPECT_EQ(1, test_wallpaper_controller_.remove_override_wallpaper_count());
  // The timer is cleared after splash screen is removed.
  EXPECT_FALSE(demo_session->GetTimerForTesting());

  // Explicitly remove the splash screen again as if the fullscreen is
  // toggled again. But it should have no effect since the splash screen
  // is already removed.
  demo_session->RemoveSplashScreen();
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count(
                   /*always_on_top=*/true));
  EXPECT_EQ(1, test_wallpaper_controller_.remove_override_wallpaper_count());
}

TEST_F(DemoSessionTest, RemoveSplashScreenWhenTimeout) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  std::unique_ptr<base::MockOneShotTimer> timer =
      std::make_unique<base::MockOneShotTimer>();
  demo_session->SetTimerForTesting(std::move(timer));

  EXPECT_EQ(0, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());

  ASSERT_TRUE(FinishResourcesComponentLoad(
      base::FilePath(kTestDemoModeResourcesMountPoint)));
  // Wait for splash screen image to load and timer to be set
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count(
                   /*always_on_top=*/true));
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());

  base::MockOneShotTimer* timer_ptr =
      static_cast<base::MockOneShotTimer*>(demo_session->GetTimerForTesting());
  ASSERT_TRUE(timer_ptr);
  timer_ptr->Fire();
  // The splash screen should be removed when the timer goes off.
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count(
                   /*always_on_top=*/true));
  EXPECT_EQ(1, test_wallpaper_controller_.remove_override_wallpaper_count());

  // Explicitly remove the splash screen again as if the fullscreen is
  // toggled. But it should have no effect since the splash screen is already
  // removed.
  demo_session->RemoveSplashScreen();
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count(
                   /*always_on_top=*/true));
  EXPECT_EQ(1, test_wallpaper_controller_.remove_override_wallpaper_count());

  // Entering active session will not trigger splash screen removal anymore.
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count(
                   /*always_on_top=*/true));
  EXPECT_EQ(1, test_wallpaper_controller_.remove_override_wallpaper_count());
}

using DemoSessionLocaleTest = DemoSessionTest;

TEST_F(DemoSessionLocaleTest, InitializeDefaultLocale) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  TestingProfile* profile = LoginDemoUser();
  // When the default locale is empty, verify that it's initialized with the
  // current locale.
  constexpr char kCurrentLocale[] = "en-US";
  profile->GetPrefs()->SetString(language::prefs::kApplicationLocale,
                                 kCurrentLocale);
  EXPECT_EQ("", TestingBrowserProcess::GetGlobal()->local_state()->GetString(
                    prefs::kDemoModeDefaultLocale));
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(kCurrentLocale,
            TestingBrowserProcess::GetGlobal()->local_state()->GetString(
                prefs::kDemoModeDefaultLocale));
  EXPECT_FALSE(profile->requested_locale().has_value());
}

TEST_F(DemoSessionLocaleTest, DefaultAndCurrentLocaleDifferent) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  TestingProfile* profile = LoginDemoUser();
  // When the default locale is different from the current locale, verify that
  // reverting to default locale is requested.
  constexpr char kCurrentLocale[] = "zh-CN";
  constexpr char kDefaultLocale[] = "en-US";
  profile->GetPrefs()->SetString(language::prefs::kApplicationLocale,
                                 kCurrentLocale);
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kDemoModeDefaultLocale, kDefaultLocale);
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(kDefaultLocale,
            TestingBrowserProcess::GetGlobal()->local_state()->GetString(
                prefs::kDemoModeDefaultLocale));
  EXPECT_EQ(kDefaultLocale, profile->requested_locale().value());
}

TEST_F(DemoSessionLocaleTest, DefaultAndCurrentLocaleIdentical) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  TestingProfile* profile = LoginDemoUser();
  // When the default locale is the same with the current locale, verify that
  // it's no-op.
  constexpr char kDefaultLocale[] = "en-US";
  profile->GetPrefs()->SetString(language::prefs::kApplicationLocale,
                                 kDefaultLocale);
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kDemoModeDefaultLocale, kDefaultLocale);
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(kDefaultLocale,
            TestingBrowserProcess::GetGlobal()->local_state()->GetString(
                prefs::kDemoModeDefaultLocale));
  EXPECT_FALSE(profile->requested_locale().has_value());
}

}  // namespace
}  // namespace ash
