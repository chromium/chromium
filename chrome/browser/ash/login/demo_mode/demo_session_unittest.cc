// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_session.h"

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ash/browser_delegate/browser_controller_impl.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/wallpaper/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/account_id/account_id.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
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
constexpr char kDemoUserEmail[] = "demo@public-accounts.device-local.localhost";

class TestUserSessionEnvironmentDelegate
    : public test::ChromeUserSessionTestEnvironmentDelegate {
 public:
  using ChromeUserSessionTestEnvironmentDelegate::
      ChromeUserSessionTestEnvironmentDelegate;

  void set_initial_locale(std::string locale) {
    initial_locale_ = std::move(locale);
  }

  // test::ChromeUserSessionTestEnvironmentDelegate:
  void CreateProfile(const AccountId& account_id) override {
    ChromeUserSessionTestEnvironmentDelegate::CreateProfile(account_id);
    // `UserSessionTestEnvironment::LogIn()` calls `SessionStarted()` right
    // after `CreateProfile()`, which synchronously fires
    // `DemoSession::OnSessionStateChanged()` and reads `kApplicationLocale`.
    if (!initial_locale_.empty()) {
      PrefService* prefs =
          CHECK_DEREF(
              user_manager::UserManager::Get()->FindUserAndModify(account_id))
              .GetProfilePrefs();
      CHECK(prefs);
      prefs->SetString(language::prefs::kApplicationLocale,
                       std::exchange(initial_locale_, std::string()));
    }
  }

 private:
  std::string initial_locale_;
};

class DemoSessionTest : public testing::Test {
 public:
  DemoSessionTest()
      : browser_process_platform_part_test_api_(
            g_browser_process->platform_part()) {
    cros_settings_test_helper_.InstallAttributes()->SetDemoMode();
    // TODO(b/321321392): Test loading growth campaigns at session start.
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kGrowthCampaignsInDemoMode);
  }

  DemoSessionTest(const DemoSessionTest&) = delete;
  DemoSessionTest& operator=(const DemoSessionTest&) = delete;

  ~DemoSessionTest() override = default;

  void SetUp() override {
    ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOnline);
    InitializeComponentManager();

    auto* browser_process = TestingBrowserProcess::GetGlobal();
    auto delegate =
        std::make_unique<TestUserSessionEnvironmentDelegate>(browser_process);
    delegate_ = delegate.get();
    user_session_test_environment_ =
        std::make_unique<test::UserSessionTestEnvironment>(
            browser_process->local_state(), std::move(delegate));
    user_manager::User* demo_user =
        user_session_test_environment_->AddPublicAccountUser(kDemoUserEmail);
    ASSERT_TRUE(demo_user);
    demo_account_id_ = demo_user->GetAccountId();

    wallpaper_controller_client_ = std::make_unique<
        WallpaperControllerClientImpl>(
        CHECK_DEREF(browser_process->local_state()),
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);
    browser_controller_ = std::make_unique<ash::BrowserControllerImpl>();
  }

  void TearDown() override {
    DemoSession::ShutDownIfInitialized();
    DemoSession::ResetDemoConfigForTesting();

    browser_controller_.reset();
    wallpaper_controller_client_.reset();
    delegate_ = nullptr;
    user_session_test_environment_.reset();

    component_manager_ash_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownComponentManager();
    ConciergeClient::Shutdown();
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

  // Logs in the demo public-account user and returns its profile.
  TestingProfile* LoginDemoUser(std::string_view initial_locale = {}) {
    delegate_->set_initial_locale(std::string(initial_locale));
    user_session_test_environment_->LogIn(demo_account_id_);
    content::BrowserContext* browser_context =
        BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            demo_account_id_);
    CHECK(browser_context);
    return static_cast<TestingProfile*>(
        Profile::FromBrowserContext(browser_context));
  }

  DemoSession* StartDemoSessionIfInDemoMode() {
    return DemoSession::StartIfInDemoMode(
        TestingBrowserProcess::GetGlobal()->local_state(),
        TestingBrowserProcess::GetGlobal()
            ->GetFeatures()
            ->application_locale_storage(),
        TestingBrowserProcess::GetGlobal()
            ->platform_part()
            ->component_manager_ash());
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  BrowserProcessPlatformPartTestApi browser_process_platform_part_test_api_;
  raw_ptr<FakeComponentManagerAsh> component_manager_ash_ = nullptr;
  std::unique_ptr<test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<TestUserSessionEnvironmentDelegate> delegate_ = nullptr;
  AccountId demo_account_id_;
  TestWallpaperController test_wallpaper_controller_;
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
  base::UserActionTester user_action_tester_;

 private:
  std::unique_ptr<BrowserControllerImpl> browser_controller_;
};

TEST_F(DemoSessionTest, StartForDeviceInDemoMode) {
  EXPECT_FALSE(DemoSession::Get());
  DemoSession* demo_session = StartDemoSessionIfInDemoMode();
  ASSERT_TRUE(demo_session);
  EXPECT_TRUE(demo_session->started());
  EXPECT_EQ(demo_session, DemoSession::Get());
}

TEST_F(DemoSessionTest, StartForDemoDeviceNotInDemoMode) {
  cros_settings_test_helper_.InstallAttributes()->SetConsumerOwned();
  EXPECT_FALSE(DemoSession::Get());
  EXPECT_FALSE(StartDemoSessionIfInDemoMode());
  EXPECT_FALSE(DemoSession::Get());

  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kResourcesComponent));
}

TEST_F(DemoSessionTest, ShutdownResetsInstance) {
  ASSERT_TRUE(StartDemoSessionIfInDemoMode());
  EXPECT_TRUE(DemoSession::Get());
  DemoSession::ShutDownIfInitialized();
  EXPECT_FALSE(DemoSession::Get());
}

TEST_F(DemoSessionTest, LoginDemoSession) {
  DemoSession* demo_session = StartDemoSessionIfInDemoMode();
  ASSERT_TRUE(demo_session);
  // There should be no user action DemoMode.DemoSessionStarts reported
  // before the user login
  EXPECT_EQ(0,
            user_action_tester_.GetActionCount("DemoMode.DemoSessionStarts"));

  LoginDemoUser();
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("DemoMode.DemoSessionStarts"));
}

TEST_F(DemoSessionTest, CannotLockScreen) {
  ASSERT_TRUE(StartDemoSessionIfInDemoMode());
  EXPECT_TRUE(DemoSession::Get());
  EXPECT_FALSE(SessionControllerClientImpl::CanLockScreen());
}

TEST_F(DemoSessionTest, ShowAndRemoveSplashScreen) {
  DemoSession* demo_session = StartDemoSessionIfInDemoMode();
  ASSERT_TRUE(demo_session);

  std::unique_ptr<base::MockOneShotTimer> timer =
      std::make_unique<base::MockOneShotTimer>();
  demo_session->SetTimerForTesting(std::move(timer));

  EXPECT_EQ(0, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());

  ASSERT_TRUE(FinishResourcesComponentLoad(
      base::FilePath(kTestDemoModeResourcesMountPoint)));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_wallpaper_controller_.show_override_wallpaper_count() == 1;
  }));
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count(
                   /*always_on_top=*/true));
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());

  LoginDemoUser();
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
  DemoSession* demo_session = StartDemoSessionIfInDemoMode();
  ASSERT_TRUE(demo_session);

  std::unique_ptr<base::MockOneShotTimer> timer =
      std::make_unique<base::MockOneShotTimer>();
  demo_session->SetTimerForTesting(std::move(timer));

  EXPECT_EQ(0, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(0, test_wallpaper_controller_.remove_override_wallpaper_count());

  ASSERT_TRUE(FinishResourcesComponentLoad(
      base::FilePath(kTestDemoModeResourcesMountPoint)));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_wallpaper_controller_.show_override_wallpaper_count() == 1;
  }));
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
  LoginDemoUser();
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count());
  EXPECT_EQ(1, test_wallpaper_controller_.show_override_wallpaper_count(
                   /*always_on_top=*/true));
  EXPECT_EQ(1, test_wallpaper_controller_.remove_override_wallpaper_count());
}

using DemoSessionLocaleTest = DemoSessionTest;

TEST_F(DemoSessionLocaleTest, InitializeDefaultLocale) {
  DemoSession* demo_session = StartDemoSessionIfInDemoMode();
  ASSERT_TRUE(demo_session);

  // When the default locale is empty, verify that it's initialized with the
  // current locale.
  constexpr char kCurrentLocale[] = "en-US";
  EXPECT_EQ("", TestingBrowserProcess::GetGlobal()->local_state()->GetString(
                    prefs::kDemoModeDefaultLocale));
  TestingProfile* profile = LoginDemoUser(kCurrentLocale);
  EXPECT_EQ(kCurrentLocale,
            TestingBrowserProcess::GetGlobal()->local_state()->GetString(
                prefs::kDemoModeDefaultLocale));
  EXPECT_FALSE(profile->requested_locale().has_value());
}

TEST_F(DemoSessionLocaleTest, DefaultAndCurrentLocaleDifferent) {
  DemoSession* demo_session = StartDemoSessionIfInDemoMode();
  ASSERT_TRUE(demo_session);

  // When the default locale is different from the current locale, verify that
  // reverting to default locale is requested.
  constexpr char kCurrentLocale[] = "zh-CN";
  constexpr char kDefaultLocale[] = "en-US";
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kDemoModeDefaultLocale, kDefaultLocale);
  TestingProfile* profile = LoginDemoUser(kCurrentLocale);
  EXPECT_EQ(kDefaultLocale,
            TestingBrowserProcess::GetGlobal()->local_state()->GetString(
                prefs::kDemoModeDefaultLocale));
  EXPECT_EQ(kDefaultLocale, profile->requested_locale().value());
}

TEST_F(DemoSessionLocaleTest, DefaultAndCurrentLocaleIdentical) {
  DemoSession* demo_session = StartDemoSessionIfInDemoMode();
  ASSERT_TRUE(demo_session);

  // When the default locale is the same with the current locale, verify that
  // it's no-op.
  constexpr char kDefaultLocale[] = "en-US";
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kDemoModeDefaultLocale, kDefaultLocale);
  TestingProfile* profile = LoginDemoUser(kDefaultLocale);
  EXPECT_EQ(kDefaultLocale,
            TestingBrowserProcess::GetGlobal()->local_state()->GetString(
                prefs::kDemoModeDefaultLocale));
  EXPECT_FALSE(profile->requested_locale().has_value());
}

}  // namespace
}  // namespace ash
