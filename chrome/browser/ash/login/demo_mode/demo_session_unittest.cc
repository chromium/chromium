// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_session.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/language/core/browser/pref_names.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

using ::component_updater::FakeCrOSComponentManager;

constexpr char kResourcesComponent[] = "demo-mode-resources";
constexpr char kTestDemoModeResourcesMountPoint[] =
    "/run/imageloader/demo_mode_resources";

class DemoSessionTest : public testing::Test {
 public:
  DemoSessionTest()
      : profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())),
        browser_process_platform_part_test_api_(
            g_browser_process->platform_part()),
        scoped_user_manager_(std::make_unique<FakeChromeUserManager>()) {}

  DemoSessionTest(const DemoSessionTest&) = delete;
  DemoSessionTest& operator=(const DemoSessionTest&) = delete;

  ~DemoSessionTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_->SetUp());
    ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOnline);
    InitializeCrosComponentManager();
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    wallpaper_controller_client_ =
        std::make_unique<WallpaperControllerClientImpl>();
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);
  }

  void TearDown() override {
    DemoSession::ShutDownIfInitialized();
    DemoSession::ResetDemoConfigForTesting();

    wallpaper_controller_client_.reset();
    ConciergeClient::Shutdown();

    cros_component_manager_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownCrosComponentManager();
    profile_manager_->DeleteAllTestingProfiles();
  }

 protected:
  bool FinishResourcesComponentLoad(const base::FilePath& mount_path) {
    EXPECT_TRUE(
        cros_component_manager_->HasPendingInstall(kResourcesComponent));
    EXPECT_TRUE(cros_component_manager_->UpdateRequested(kResourcesComponent));

    return cros_component_manager_->FinishLoadRequest(
        kResourcesComponent,
        FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/dev/null"), mount_path));
  }

  void InitializeCrosComponentManager() {
    auto fake_cros_component_manager =
        base::MakeRefCounted<FakeCrOSComponentManager>();
    fake_cros_component_manager->set_queue_load_requests(true);
    fake_cros_component_manager->set_supported_components(
        {kResourcesComponent});
    cros_component_manager_ = fake_cros_component_manager.get();

    browser_process_platform_part_test_api_.InitializeCrosComponentManager(
        std::move(fake_cros_component_manager));
  }

  // Creates a dummy demo user with a testing profile and logs in.
  TestingProfile* LoginDemoUser() {
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("demo@test.com", "demo_user"));
    FakeChromeUserManager* user_manager =
        static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
    user_manager->AddPublicAccountUser(account_id);

    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        account_id.GetUserEmail(), std::move(prefs), u"Test profile",
        /*avatar_id=*/1, TestingProfile::TestingFactories());

    user_manager->LoginUser(account_id);
    return profile;
  }

  FakeCrOSComponentManager* cros_component_manager_ = nullptr;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
  TestWallpaperController test_wallpaper_controller_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  BrowserProcessPlatformPartTestApi browser_process_platform_part_test_api_;
  user_manager::ScopedUserManager scoped_user_manager_;
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;
};

TEST_F(DemoSessionTest, StartForDeviceInDemoMode) {
  EXPECT_FALSE(DemoSession::Get());
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);
  EXPECT_TRUE(demo_session->started());
  EXPECT_EQ(demo_session, DemoSession::Get());
}

TEST_F(DemoSessionTest, StartForDemoDeviceNotInDemoMode) {
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kNone);
  EXPECT_FALSE(DemoSession::Get());
  EXPECT_FALSE(DemoSession::StartIfInDemoMode());
  EXPECT_FALSE(DemoSession::Get());

  EXPECT_FALSE(cros_component_manager_->HasPendingInstall(kResourcesComponent));
}

TEST_F(DemoSessionTest, ShutdownResetsInstance) {
  ASSERT_TRUE(DemoSession::StartIfInDemoMode());
  EXPECT_TRUE(DemoSession::Get());
  DemoSession::ShutDownIfInitialized();
  EXPECT_FALSE(DemoSession::Get());
}

TEST_F(DemoSessionTest, ShowAndRemoveSplashScreen) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  std::unique_ptr<base::MockOneShotTimer> timer =
      std::make_unique<base::MockOneShotTimer>();
  demo_session->SetTimerForTesting(std::move(timer));

  EXPECT_EQ(0, test_wallpaper_controller_.show_always_on_top_wallpaper_count());
  EXPECT_EQ(0,
            test_wallpaper_controller_.remove_always_on_top_wallpaper_count());
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, test_wallpaper_controller_.show_always_on_top_wallpaper_count());
  EXPECT_EQ(0,
            test_wallpaper_controller_.remove_always_on_top_wallpaper_count());

  ASSERT_TRUE(FinishResourcesComponentLoad(
      base::FilePath(kTestDemoModeResourcesMountPoint)));
  // Wait for splash screen image to load and timer to be set
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_wallpaper_controller_.show_always_on_top_wallpaper_count());
  EXPECT_EQ(0,
            test_wallpaper_controller_.remove_always_on_top_wallpaper_count());

  TestingProfile* profile = LoginDemoUser();
  scoped_refptr<const extensions::Extension> screensaver_app =
      extensions::ExtensionBuilder()
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", "Test App")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Build())
          .SetID(DemoSession::GetScreensaverAppId())
          .Build();
  extensions::AppWindow* app_window = new extensions::AppWindow(
      profile,
      std::make_unique<ChromeAppDelegate>(profile, true /* keep_alive */),
      screensaver_app.get());
  demo_session->OnAppWindowActivated(app_window);
  // The splash screen is not removed until active session starts.
  EXPECT_EQ(1, test_wallpaper_controller_.show_always_on_top_wallpaper_count());
  EXPECT_EQ(0,
            test_wallpaper_controller_.remove_always_on_top_wallpaper_count());
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(1, test_wallpaper_controller_.show_always_on_top_wallpaper_count());
  EXPECT_EQ(1,
            test_wallpaper_controller_.remove_always_on_top_wallpaper_count());
  // The timer is cleared after splash screen is removed.
  EXPECT_FALSE(demo_session->GetTimerForTesting());

  app_window->OnNativeClose();
}

TEST_F(DemoSessionTest, RemoveSplashScreenWhenTimeout) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  std::unique_ptr<base::MockOneShotTimer> timer =
      std::make_unique<base::MockOneShotTimer>();
  demo_session->SetTimerForTesting(std::move(timer));

  EXPECT_EQ(0, test_wallpaper_controller_.show_always_on_top_wallpaper_count());
  EXPECT_EQ(0,
            test_wallpaper_controller_.remove_always_on_top_wallpaper_count());
  session_manager_->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(0, test_wallpaper_controller_.show_always_on_top_wallpaper_count());
  EXPECT_EQ(0,
            test_wallpaper_controller_.remove_always_on_top_wallpaper_count());

  ASSERT_TRUE(FinishResourcesComponentLoad(
      base::FilePath(kTestDemoModeResourcesMountPoint)));
  // Wait for splash screen image to load and timer to be set
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_wallpaper_controller_.show_always_on_top_wallpaper_count());
  EXPECT_EQ(0,
            test_wallpaper_controller_.remove_always_on_top_wallpaper_count());

  base::MockOneShotTimer* timer_ptr =
      static_cast<base::MockOneShotTimer*>(demo_session->GetTimerForTesting());
  ASSERT_TRUE(timer_ptr);
  timer_ptr->Fire();
  EXPECT_EQ(1, test_wallpaper_controller_.show_always_on_top_wallpaper_count());
  EXPECT_EQ(1,
            test_wallpaper_controller_.remove_always_on_top_wallpaper_count());

  // Launching the screensaver will not trigger splash screen removal anymore.
  TestingProfile* profile = LoginDemoUser();
  scoped_refptr<const extensions::Extension> screensaver_app =
      extensions::ExtensionBuilder()
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", "Test App")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Build())
          .SetID(DemoSession::GetScreensaverAppId())
          .Build();
  extensions::AppWindow* app_window = new extensions::AppWindow(
      profile,
      std::make_unique<ChromeAppDelegate>(profile, true /* keep_alive */),
      screensaver_app.get());
  demo_session->OnAppWindowActivated(app_window);
  EXPECT_EQ(1, test_wallpaper_controller_.show_always_on_top_wallpaper_count());
  EXPECT_EQ(1,
            test_wallpaper_controller_.remove_always_on_top_wallpaper_count());
  // Entering active session will not trigger splash screen removal anymore.
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(1, test_wallpaper_controller_.show_always_on_top_wallpaper_count());
  EXPECT_EQ(1,
            test_wallpaper_controller_.remove_always_on_top_wallpaper_count());

  app_window->OnNativeClose();
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
