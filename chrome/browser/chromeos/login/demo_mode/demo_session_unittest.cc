// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using component_updater::FakeCrOSComponentManager;

namespace chromeos {

namespace {

// TODO(michaelpg): Clean up tests for offline resources and differentiate
// between the CrOS component and the preinstalled resources image.
constexpr char kOfflineResourcesComponent[] = "demo-mode-resources";
constexpr char kTestDemoModeResourcesMountPoint[] =
    "/run/imageloader/demo_mode_resources";
constexpr char kDemoAppsImageFile[] = "android_demo_apps.squash";
constexpr char kExternalExtensionsPrefsFile[] = "demo_extensions.json";

void SetBoolean(bool* value) {
  *value = true;
}

}  // namespace

class DemoSessionTest : public testing::Test {
 public:
  DemoSessionTest()
      : browser_process_platform_part_test_api_(
            g_browser_process->platform_part()) {}
  ~DemoSessionTest() override = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOnline);
    InitializeCrosComponentManager();
    session_manager_ = std::make_unique<session_manager::SessionManager>();
  }

  void TearDown() override {
    DemoSession::ShutDownIfInitialized();
    DemoSession::ResetDemoConfigForTesting();

    chromeos::DBusThreadManager::Shutdown();

    cros_component_manager_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownCrosComponentManager();
  }

 protected:
  bool FinishResourcesComponentLoad(const base::FilePath& mount_path) {
    EXPECT_TRUE(
        cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
    EXPECT_FALSE(
        cros_component_manager_->UpdateRequested(kOfflineResourcesComponent));

    return cros_component_manager_->FinishLoadRequest(
        kOfflineResourcesComponent,
        FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/dev/null"), mount_path));
  }

  void InitializeCrosComponentManager() {
    auto fake_cros_component_manager =
        std::make_unique<FakeCrOSComponentManager>();
    fake_cros_component_manager->set_queue_load_requests(true);
    fake_cros_component_manager->set_supported_components(
        {kOfflineResourcesComponent});
    cros_component_manager_ = fake_cros_component_manager.get();

    browser_process_platform_part_test_api_.InitializeCrosComponentManager(
        std::move(fake_cros_component_manager));
  }

  FakeCrOSComponentManager* cros_component_manager_ = nullptr;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;

 private:
  BrowserProcessPlatformPartTestApi browser_process_platform_part_test_api_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionTest);
};

TEST_F(DemoSessionTest, StartForDeviceInDemoMode) {
  EXPECT_FALSE(DemoSession::Get());
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);
  EXPECT_TRUE(demo_session->started());
  EXPECT_FALSE(demo_session->offline_enrolled());
  EXPECT_EQ(demo_session, DemoSession::Get());
}

TEST_F(DemoSessionTest, StartInitiatesOfflineResourcesLoad) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));

  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
  EXPECT_EQ(
      component_mount_point.AppendASCII("foo.txt"),
      demo_session->GetOfflineResourceAbsolutePath(base::FilePath("foo.txt")));
  EXPECT_EQ(component_mount_point.AppendASCII("foo/bar.txt"),
            demo_session->GetOfflineResourceAbsolutePath(
                base::FilePath("foo/bar.txt")));
  EXPECT_EQ(
      component_mount_point.AppendASCII("foo/"),
      demo_session->GetOfflineResourceAbsolutePath(base::FilePath("foo/")));
  EXPECT_TRUE(
      demo_session->GetOfflineResourceAbsolutePath(base::FilePath("../foo/"))
          .empty());
  EXPECT_TRUE(
      demo_session->GetOfflineResourceAbsolutePath(base::FilePath("foo/../bar"))
          .empty());
}

TEST_F(DemoSessionTest, StartForDemoDeviceNotInDemoMode) {
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kNone);
  EXPECT_FALSE(DemoSession::Get());
  EXPECT_FALSE(DemoSession::StartIfInDemoMode());
  EXPECT_FALSE(DemoSession::Get());

  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
}

TEST_F(DemoSessionTest, StartIfInOfflineEnrolledDemoMode) {
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOffline);

  EXPECT_FALSE(DemoSession::Get());
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);
  EXPECT_TRUE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_enrolled());
  EXPECT_EQ(demo_session, DemoSession::Get());

  EXPECT_FALSE(demo_session->offline_resources_loaded());
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
}

TEST_F(DemoSessionTest, PreloadOfflineResourcesIfInDemoMode) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();

  DemoSession* demo_session = DemoSession::Get();
  ASSERT_TRUE(demo_session);
  EXPECT_FALSE(demo_session->started());
  EXPECT_FALSE(demo_session->offline_enrolled());

  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_FALSE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
}

TEST_F(DemoSessionTest, PreloadOfflineResourcesIfNotInDemoMode) {
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kNone);
  DemoSession::PreloadOfflineResourcesIfInDemoMode();
  EXPECT_FALSE(DemoSession::Get());
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
}

TEST_F(DemoSessionTest, PreloadOfflineResourcesIfInOfflineDemoMode) {
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOffline);
  DemoSession::PreloadOfflineResourcesIfInDemoMode();

  DemoSession* demo_session = DemoSession::Get();
  ASSERT_TRUE(demo_session);
  EXPECT_FALSE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_enrolled());

  EXPECT_FALSE(demo_session->offline_resources_loaded());
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
}

TEST_F(DemoSessionTest, ShutdownResetsInstance) {
  ASSERT_TRUE(DemoSession::StartIfInDemoMode());
  EXPECT_TRUE(DemoSession::Get());
  DemoSession::ShutDownIfInitialized();
  EXPECT_FALSE(DemoSession::Get());
}

TEST_F(DemoSessionTest, ShutdownAfterPreload) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();
  EXPECT_TRUE(DemoSession::Get());
  DemoSession::ShutDownIfInitialized();
  EXPECT_FALSE(DemoSession::Get());
}

TEST_F(DemoSessionTest, StartDemoSessionWhilePreloadingResources) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();

  ASSERT_TRUE(demo_session);
  EXPECT_TRUE(demo_session->started());

  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_TRUE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
}

TEST_F(DemoSessionTest, StartDemoSessionAfterPreloadingResources) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  EXPECT_TRUE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());

  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
}

TEST_F(DemoSessionTest, EnsureOfflineResourcesLoadedAfterStart) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  bool callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &callback_called));

  EXPECT_FALSE(callback_called);
  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
}

TEST_F(DemoSessionTest, EnsureOfflineResourcesLoadedAfterOfflineResourceLoad) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  bool callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &callback_called));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
}

TEST_F(DemoSessionTest, EnsureOfflineResourcesLoadedAfterPreload) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();

  DemoSession* demo_session = DemoSession::Get();
  ASSERT_TRUE(demo_session);

  bool callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &callback_called));

  EXPECT_FALSE(callback_called);
  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
}

TEST_F(DemoSessionTest, MultipleEnsureOfflineResourcesLoaded) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  bool first_callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &first_callback_called));

  bool second_callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &second_callback_called));

  bool third_callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &third_callback_called));

  EXPECT_FALSE(first_callback_called);
  EXPECT_FALSE(second_callback_called);
  EXPECT_FALSE(third_callback_called);
  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_TRUE(first_callback_called);
  EXPECT_TRUE(second_callback_called);
  EXPECT_TRUE(third_callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
}

class DemoSessionLocaleTest : public DemoSessionTest {
 public:
  DemoSessionLocaleTest() {
    auto fake_user_manager = std::make_unique<FakeChromeUserManager>();
    user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  ~DemoSessionLocaleTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    DemoSessionTest::SetUp();
  }

  void TearDown() override {
    profile_manager_->DeleteAllTestingProfiles();
    DemoSessionTest::TearDown();
  }

 protected:
  // Creates a dummy demo user with a testing profile and logs in.
  TestingProfile* LoginDemoUser() {
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("demo@test.com", "demo_user"));
    const user_manager::User* user =
        user_manager_->AddPublicAccountUser(account_id);

    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        "test-profile", std::move(prefs), base::ASCIIToUTF16("Test profile"),
        1 /* avatar_id */, std::string() /* supervised_user_id */,
        TestingProfile::TestingFactories());
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                      profile);

    user_manager_->LoginUser(account_id);
    profile_manager_->SetLoggedIn(true);
    return profile;
  }

 private:
  FakeChromeUserManager* user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionLocaleTest);
};

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

}  // namespace chromeos
