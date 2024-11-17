// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_mode_manager.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#endif

using extensions::mojom::ManifestLocation;
using testing::_;
using testing::AtMost;
using testing::Exactly;
using testing::InSequence;
using testing::Mock;
using testing::NiceMock;
using testing::StrictMock;

namespace {

std::unique_ptr<TestingProfileManager> CreateTestingProfileManager() {
  std::unique_ptr<TestingProfileManager> profile_manager(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  EXPECT_TRUE(profile_manager->SetUp());
  return profile_manager;
}

// Helper class that tracks state transitions in BackgroundModeManager and
// exposes them via getters (or gmock for EnableLaunchOnStartup).
class TestBackgroundModeManager : public StrictMock<BackgroundModeManager> {
 public:
  TestBackgroundModeManager(const base::CommandLine& command_line,
                            ProfileAttributesStorage* storage)
      : StrictMock<BackgroundModeManager>(command_line, storage) {
    ResumeBackgroundMode();
  }
  TestBackgroundModeManager(TestBackgroundModeManager&) = delete;
  TestBackgroundModeManager& operator=(TestBackgroundModeManager&) = delete;
  ~TestBackgroundModeManager() override = default;

  MOCK_METHOD1(EnableLaunchOnStartup, void(bool should_launch));

  // TODO: Use strict-mocking rather than keeping state through overrides below.
  void DisplayClientInstalledNotification(const std::u16string& name) override {
    has_shown_balloon_ = true;
  }
  void CreateStatusTrayIcon() override { have_status_tray_ = true; }
  void RemoveStatusTrayIcon() override { have_status_tray_ = false; }

  bool HaveStatusTray() const { return have_status_tray_; }
  bool HasShownBalloon() const { return has_shown_balloon_; }
  void SetHasShownBalloon(bool value) { has_shown_balloon_ = value; }

 private:
  // Flags to track whether we have a status tray/have shown the balloon.
  bool have_status_tray_ = false;
  bool has_shown_balloon_ = false;
};

class TestStatusIcon : public StatusIcon {
 public:
  TestStatusIcon() = default;
  TestStatusIcon(TestStatusIcon&) = delete;
  TestStatusIcon& operator=(TestStatusIcon&) = delete;

  void SetImage(const gfx::ImageSkia& image) override {}
  void SetToolTip(const std::u16string& tool_tip) override {}
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override {}
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {}
};

void AssertBackgroundModeActive(const TestBackgroundModeManager& manager) {
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  EXPECT_TRUE(manager.HaveStatusTray());
}

void AssertBackgroundModeInactive(const TestBackgroundModeManager& manager) {
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  EXPECT_FALSE(manager.HaveStatusTray());
}

}  // namespace

// More complex test helper that exposes APIs for fine grained control of
// things like the number of background applications. This allows writing
// smaller tests that don't have to install/uninstall extensions.
class AdvancedTestBackgroundModeManager : public TestBackgroundModeManager {
 public:
  AdvancedTestBackgroundModeManager(const base::CommandLine& command_line,
                                    ProfileAttributesStorage* storage,
                                    bool enabled)
      : TestBackgroundModeManager(command_line, storage), enabled_(enabled) {}
  AdvancedTestBackgroundModeManager(AdvancedTestBackgroundModeManager&) =
      delete;
  AdvancedTestBackgroundModeManager& operator=(
      AdvancedTestBackgroundModeManager&) = delete;
  ~AdvancedTestBackgroundModeManager() override = default;

  // TestBackgroundModeManager:
  bool HasPersistentBackgroundClient() const override {
    return base::ranges::any_of(
        profile_app_counts_, [](const auto& profile_count_pair) {
          return profile_count_pair.second.persistent > 0;
        });
  }
  bool HasAnyBackgroundClient() const override {
    return base::ranges::any_of(profile_app_counts_,
                                [](const auto& profile_count_pair) {
                                  return profile_count_pair.second.any > 0;
                                });
  }
  bool HasPersistentBackgroundClientForProfile(
      const Profile* profile) const override {
    auto it = profile_app_counts_.find(profile);
    if (it == profile_app_counts_.end()) {
      ADD_FAILURE();
      return false;
    }
    return it->second.persistent > 0;
  }

  bool IsBackgroundModePrefEnabled() const override { return enabled_; }

  void SetBackgroundClientCountForProfile(const Profile* profile,
                                          size_t count) {
    profile_app_counts_[profile] = {count, count};
  }

  void SetPersistentBackgroundClientCountForProfile(const Profile* profile,
                                                    size_t count) {
    profile_app_counts_[profile].persistent = count;
  }

  void SetEnabled(bool enabled) {
    enabled_ = enabled;
    OnBackgroundModeEnabledPrefChanged();
  }

  using BackgroundModeManager::OnApplicationListChanged;

 private:
  struct AppCounts {
    size_t any = 0;
    size_t persistent = 0;
  };
  bool enabled_;
  std::map<const Profile*, AppCounts> profile_app_counts_;
};

class BackgroundModeManagerTest : public testing::Test {
 public:
  BackgroundModeManagerTest() = default;
  BackgroundModeManagerTest(BackgroundModeManagerTest&) = delete;
  BackgroundModeManagerTest& operator=(BackgroundModeManagerTest&) = delete;
  ~BackgroundModeManagerTest() override = default;

  void SetUp() override {
    command_line_ =
        std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);

    auto policy_service = std::make_unique<policy::PolicyServiceImpl>(
        std::vector<
            raw_ptr<policy::ConfigurationPolicyProvider, VectorExperimental>>{
            &policy_provider_});
    profile_manager_ = CreateTestingProfileManager();
    profile_ = profile_manager_->CreateTestingProfile(
        "p1", nullptr, u"p1", 0, TestingProfile::TestingFactories(),
        /*is_supervised_profile=*/false, std::nullopt,
        std::move(policy_service));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<base::CommandLine> command_line_;

  NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Test profile used by all tests - this is owned by profile_manager_.
  raw_ptr<TestingProfile> profile_;
};

class BackgroundModeManagerWithExtensionsTest : public testing::Test {
 public:
  BackgroundModeManagerWithExtensionsTest() = default;
  BackgroundModeManagerWithExtensionsTest(
      BackgroundModeManagerWithExtensionsTest&) = delete;
  BackgroundModeManagerWithExtensionsTest& operator=(
      BackgroundModeManagerWithExtensionsTest&) = delete;
  ~BackgroundModeManagerWithExtensionsTest() override = default;

  void SetUp() override {
    command_line_ =
        std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
    profile_manager_ = CreateTestingProfileManager();
    profile_ = profile_manager_->CreateTestingProfile("p1");

    test_keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BACKGROUND_MODE_MANAGER,
        KeepAliveRestartOption::DISABLED);

    // Create our test BackgroundModeManager.
    manager_ = std::make_unique<TestBackgroundModeManager>(
        *command_line_, profile_manager_->profile_attributes_storage());
    manager_->RegisterProfile(profile_);
  }

  void TearDown() override {
    // Clean up the status icon. If this is not done before profile deletes,
    // the context menu updates will DCHECK with the now deleted profiles.
    delete manager_->status_icon_;
    manager_->status_icon_ = nullptr;

    // We're getting ready to shutdown the message loop. Clear everything out!
    base::RunLoop().RunUntilIdle();

    // TestBackgroundModeManager has dependencies on the infrastructure.
    // It should get cleared first.
    manager_.reset();

    // Now that the background manager is destroyed, the test KeepAlive can be
    // cleared without having |manager_| attempt to perform optimizations.
    test_keep_alive_.reset();

    // The Profile Manager references the Browser Process.
    // The Browser Process references the Notification UI Manager.
    // The Notification UI Manager references the Message Center.
    // As a result, we have to clear the browser process state here
    // before tearing down the Message Center.
    profile_manager_.reset();

    // Clear the shutdown flag to isolate the remaining effect of this test.
    browser_shutdown::SetTryingToQuit(false);
  }

 protected:
  // From views::MenuModelAdapter::IsCommandEnabled with modification.
  bool IsCommandEnabled(ui::MenuModel* model, int id) const {
    size_t index = 0;
    return ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index) &&
           model->IsEnabledAt(index);
  }

  std::unique_ptr<TestBackgroundModeManager> manager_;

  std::unique_ptr<base::CommandLine> command_line_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Test profile used by all tests - this is owned by profile_manager_.
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;

 private:
  // Required for extension service.
  content::BrowserTaskEnvironment task_environment_;

  // BackgroundModeManager actually affects Chrome start/stop state,
  // tearing down our thread bundle before we've had chance to clean
  // everything up. Keeping Chrome alive prevents this.
  // We aren't interested in if the keep alive works correctly in this test.
  std::unique_ptr<ScopedKeepAlive> test_keep_alive_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS needs extra services to run in the following order.
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<UserManagerDelegateImpl>(),
          g_browser_process->local_state(),
          CrosSettings::Get())};
#endif
};


TEST_F(BackgroundModeManagerTest, BackgroundAppLoadUnload) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Mimic app load.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundClientInstalled(u"name");
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);

  manager.SuspendBackgroundMode();
  AssertBackgroundModeInactive(manager);
  manager.ResumeBackgroundMode();

  // Mimic app unload.
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetBackgroundClientCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeInactive(manager);

  manager.SuspendBackgroundMode();
  AssertBackgroundModeInactive(manager);

  // Mimic app load while suspended, e.g. from sync. This should enable and
  // resume background mode.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundClientInstalled(u"name");
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);
}

// Apps installed while background mode is disabled should cause activation
// after it is enabled - crbug.com/527023.
TEST_F(BackgroundModeManagerTest, DISABLED_BackgroundAppInstallWhileDisabled) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);

  // Turn off background mode (shouldn't explicitly disable launch-on-startup as
  // the app-count is zero and launch-on-startup shouldn't be considered on).
  manager.SetEnabled(false);
  AssertBackgroundModeInactive(manager);

  // When a new client is installed, status tray icons will not be created,
  // launch on startup status will not be modified.
  manager.OnBackgroundClientInstalled(u"name");
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeInactive(manager);

  // Turn back on background mode, should show status tray icon as there is now
  // an app installed.
  manager.SetEnabled(true);
  AssertBackgroundModeActive(manager);
}

// Apps installed and uninstalled while background mode is disabled should do
// nothing.
TEST_F(BackgroundModeManagerTest, BackgroundAppInstallUninstallWhileDisabled) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);

  // Turn off background mode (should explicitly disable launch-on-startup as
  // the app-count is zero and launch-on-startup hasn't been initialized yet).
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetEnabled(false);
  AssertBackgroundModeInactive(manager);
  Mock::VerifyAndClearExpectations(&manager);

  // When a new client is installed, status tray icons will not be created,
  // launch on startup status will not be modified.
  manager.OnBackgroundClientInstalled(u"name");
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeInactive(manager);

  manager.SetBackgroundClientCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeInactive(manager);

  // Re-enable background mode (shouldn't actually enable launch-on-startup as
  // the app-count is zero).
  manager.SetEnabled(true);
  AssertBackgroundModeInactive(manager);
}

// Apps installed before background mode is disabled cause the icon to show up
// again when it is enabled.
TEST_F(BackgroundModeManagerTest, EnableAfterBackgroundAppInstall) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);

  // Install app, should show status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundClientInstalled(u"name");
  // OnBackgroundClientInstalled does not actually add an app to the
  // BackgroundApplicationListModel which would result in another
  // call to CreateStatusTray.
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeActive(manager);
  Mock::VerifyAndClearExpectations(&manager);

  // Turn off background mode - should hide status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetEnabled(false);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeInactive(manager);

  // Turn back on background mode, should show status tray icon again as there
  // was already an app installed before background mode was disabled.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.SetEnabled(true);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);

  // Uninstall app, should hide status tray icon again.
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetBackgroundClientCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeInactive(manager);
}

TEST_F(BackgroundModeManagerTest, MultiProfile) {
  TestingProfile* profile2 = profile_manager_->CreateTestingProfile("p2");
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);
  manager.RegisterProfile(profile2);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Install app, should show status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundClientInstalled(u"name");
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);

  // Install app for other profile, should show other status tray icon.
  manager.OnBackgroundClientInstalled(u"name");
  manager.SetBackgroundClientCountForProfile(profile2, 2);
  manager.OnApplicationListChanged(profile2);
  AssertBackgroundModeActive(manager);

  // Should hide both status tray icons.
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetEnabled(false);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeInactive(manager);

  // Turn back on background mode - should show both status tray icons.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.SetEnabled(true);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);

  manager.SetBackgroundClientCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  manager.SetBackgroundClientCountForProfile(profile2, 1);
  manager.OnApplicationListChanged(profile2);
  // There is still one background app alive
  AssertBackgroundModeActive(manager);
  // Verify the implicit expectations of no calls on this StrictMock.
  Mock::VerifyAndClearExpectations(&manager);

  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetBackgroundClientCountForProfile(profile2, 0);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeInactive(manager);
}

TEST_F(BackgroundModeManagerTest, ProfileAttributesStorage) {
  TestingProfile* profile2 = profile_manager_->CreateTestingProfile("p2");
  ProfileAttributesStorage* storage =
      profile_manager_->profile_attributes_storage();
  AdvancedTestBackgroundModeManager manager(*command_line_, storage, true);
  manager.RegisterProfile(profile_);
  manager.RegisterProfile(profile2);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  EXPECT_EQ(2u, storage->GetNumberOfProfiles());

  ProfileAttributesEntry* entry1 =
      storage->GetProfileAttributesWithPath(profile_->GetPath());
  ASSERT_NE(entry1, nullptr);
  ProfileAttributesEntry* entry2 =
      storage->GetProfileAttributesWithPath(profile2->GetPath());
  ASSERT_NE(entry2, nullptr);

  EXPECT_FALSE(entry1->GetBackgroundStatus());
  EXPECT_FALSE(entry2->GetBackgroundStatus());

  // Install app, should show status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundClientInstalled(u"name");
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);

  // Install app for other profile.
  manager.OnBackgroundClientInstalled(u"name");
  manager.SetBackgroundClientCountForProfile(profile2, 1);
  manager.OnApplicationListChanged(profile2);

  EXPECT_TRUE(entry1->GetBackgroundStatus());
  EXPECT_TRUE(entry2->GetBackgroundStatus());

  manager.SetBackgroundClientCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);

  EXPECT_FALSE(entry1->GetBackgroundStatus());

  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetBackgroundClientCountForProfile(profile2, 0);
  manager.OnApplicationListChanged(profile2);
  Mock::VerifyAndClearExpectations(&manager);

  EXPECT_FALSE(entry2->GetBackgroundStatus());

  // Even though neither has background status on, there should still be two
  // profiles in the ProfileAttributesStorage.
  EXPECT_EQ(2u, storage->GetNumberOfProfiles());
}

TEST_F(BackgroundModeManagerTest, ProfileAttributesStorageObserver) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Install app, should show status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundClientInstalled(u"name");
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);

  // Background mode should remain active for the remainder of this test.

  manager.OnProfileNameChanged(
      profile_->GetPath(),
      manager.GetBackgroundModeData(profile_)->name());

  EXPECT_EQ(u"p1", manager.GetBackgroundModeData(profile_)->name());

  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  TestingProfile* profile2 = profile_manager_->CreateTestingProfile("p2");
  manager.RegisterProfile(profile2);
  EXPECT_EQ(2U, manager.NumberOfBackgroundModeData());

  manager.OnProfileAdded(profile2->GetPath());
  EXPECT_EQ(u"p2", manager.GetBackgroundModeData(profile2)->name());

  manager.OnProfileWillBeRemoved(profile2->GetPath());
  // Should still be in background mode after deleting profile.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  EXPECT_EQ(1U, manager.NumberOfBackgroundModeData());

  // Check that the background mode data we think is in the map actually is.
  EXPECT_EQ(u"p1", manager.GetBackgroundModeData(profile_)->name());
}

TEST_F(BackgroundModeManagerTest, DeleteBackgroundProfile) {
  // Tests whether deleting the only profile when it is a BG profile works
  // or not (http://crbug.com/346214).
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Install app, should show status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundClientInstalled(u"name");
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);

  manager.OnProfileNameChanged(
      profile_->GetPath(),
      manager.GetBackgroundModeData(profile_)->name());

  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  manager.SetBackgroundClientCountForProfile(profile_, 0);
  manager.OnProfileWillBeRemoved(profile_->GetPath());
  Mock::VerifyAndClearExpectations(&manager);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
}

TEST_F(BackgroundModeManagerTest, DisableBackgroundModeUnderTestFlag) {
  command_line_->AppendSwitch(switches::kKeepAliveForTest);
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);
  EXPECT_TRUE(manager.ShouldBeInBackgroundMode());

  // No enable-launch-on-startup calls expected yet.
  Mock::VerifyAndClearExpectations(&manager);
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetEnabled(false);
  EXPECT_FALSE(manager.ShouldBeInBackgroundMode());
}

TEST_F(BackgroundModeManagerTest,
       BackgroundModeDisabledPreventsKeepAliveOnStartup) {
  command_line_->AppendSwitch(switches::kKeepAliveForTest);
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), false);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(manager.ShouldBeInBackgroundMode());
}

TEST_F(BackgroundModeManagerWithExtensionsTest, BackgroundMenuGeneration) {
  scoped_refptr<const extensions::Extension> component_extension =
      extensions::ExtensionBuilder("Component Extension")
          .SetLocation(ManifestLocation::kComponent)
          .AddAPIPermission("background")
          .Build();

  scoped_refptr<const extensions::Extension> component_extension_with_options =
      extensions::ExtensionBuilder("Component Extension with Options")
          .SetLocation(ManifestLocation::kComponent)
          .AddAPIPermission("background")
          .SetManifestKey("options_page", "test.html")
          .Build();

  scoped_refptr<const extensions::Extension> regular_extension =
      extensions::ExtensionBuilder("Regular Extension")
          .SetLocation(ManifestLocation::kCommandLine)
          .AddAPIPermission("background")
          .Build();

  scoped_refptr<const extensions::Extension> regular_extension_with_options =
      extensions::ExtensionBuilder("Regular Extension with Options")
          .SetLocation(ManifestLocation::kCommandLine)
          .AddAPIPermission("background")
          .SetManifestKey("options_page", "test.html")
          .Build();

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_))
      ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                               base::FilePath(), false);
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service->Init();
  // ExtensionSystem::ready() is dispatched using PostTask to UI Thread. Wait
  // until idle so that BackgroundApplicationListModel::OnExtensionSystemReady
  // called.
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*manager_, EnableLaunchOnStartup(true)).Times(Exactly(1));
  service->AddComponentExtension(component_extension.get());
  service->AddComponentExtension(component_extension_with_options.get());
  service->AddExtension(regular_extension.get());
  service->AddExtension(regular_extension_with_options.get());
  Mock::VerifyAndClearExpectations(manager_.get());

  auto menu = std::make_unique<StatusIconMenuModel>(nullptr);
  auto submenu = std::make_unique<StatusIconMenuModel>(nullptr);
  BackgroundModeManager::BackgroundModeData* bmd =
      manager_->GetBackgroundModeData(profile_);
  bmd->BuildProfileMenu(submenu.get(), menu.get());
  EXPECT_EQ(submenu->GetLabelAt(0), u"Component Extension");
  EXPECT_FALSE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(0)));
  EXPECT_EQ(submenu->GetLabelAt(1), u"Component Extension with Options");
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(1)));
  EXPECT_EQ(submenu->GetLabelAt(2), u"Regular Extension");
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(2)));
  EXPECT_EQ(submenu->GetLabelAt(3), u"Regular Extension with Options");
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(3)));
}

TEST_F(BackgroundModeManagerWithExtensionsTest,
       BackgroundMenuGenerationMultipleProfile) {
  // Helper methods to build extensions; we build new instances so that each
  // Extension object is only used in a single profile.
  auto build_component_extension = []() {
    return extensions::ExtensionBuilder("Component Extension")
        .SetLocation(ManifestLocation::kComponent)
        .AddAPIPermission("background")
        .Build();
  };
  auto build_component_extension_with_options = []() {
    return extensions::ExtensionBuilder("Component Extension with Options")
        .SetLocation(ManifestLocation::kComponent)
        .AddAPIPermission("background")
        .SetManifestKey("options_page", "test.html")
        .Build();
  };
  auto build_regular_extension = []() {
    return extensions::ExtensionBuilder("Regular Extension")
        .SetLocation(ManifestLocation::kCommandLine)
        .AddAPIPermission("background")
        .Build();
  };
  auto build_regular_extension_with_options = []() {
    return extensions::ExtensionBuilder("Regular Extension with Options")
        .SetLocation(ManifestLocation::kCommandLine)
        .AddAPIPermission("background")
        .SetManifestKey("options_page", "test.html")
        .Build();
  };

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_))
      ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                               base::FilePath(), false);
  extensions::ExtensionService* service1 =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service1->Init();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*manager_, EnableLaunchOnStartup(true)).Times(Exactly(1));
  service1->AddComponentExtension(build_component_extension().get());
  service1->AddComponentExtension(
      build_component_extension_with_options().get());
  service1->AddExtension(build_regular_extension().get());
  service1->AddExtension(build_regular_extension_with_options().get());
  Mock::VerifyAndClearExpectations(manager_.get());

  TestingProfile* profile2 = profile_manager_->CreateTestingProfile("p2");
  manager_->RegisterProfile(profile2);

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile2))
      ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                               base::FilePath(), false);
  extensions::ExtensionService* service2 =
      extensions::ExtensionSystem::Get(profile2)->extension_service();
  service2->Init();
  base::RunLoop().RunUntilIdle();

  service2->AddComponentExtension(build_component_extension().get());
  service2->AddExtension(build_regular_extension().get());
  service2->AddExtension(build_regular_extension_with_options().get());

  manager_->status_icon_ = new TestStatusIcon();
  manager_->UpdateStatusTrayIconContextMenu();
  StatusIconMenuModel* context_menu = manager_->context_menu_;
  EXPECT_TRUE(context_menu);

  // Background Profile Enable Checks
  EXPECT_EQ(context_menu->GetLabelAt(3), u"p1");
  EXPECT_TRUE(
      context_menu->IsCommandIdEnabled(context_menu->GetCommandIdAt(3)));
  EXPECT_EQ(context_menu->GetCommandIdAt(3), 4);

  EXPECT_EQ(context_menu->GetLabelAt(4), u"p2");
  EXPECT_TRUE(
      context_menu->IsCommandIdEnabled(context_menu->GetCommandIdAt(4)));
  EXPECT_EQ(context_menu->GetCommandIdAt(4), 8);

  // Profile 1 Submenu Checks
  StatusIconMenuModel* profile1_submenu =
      static_cast<StatusIconMenuModel*>(context_menu->GetSubmenuModelAt(3));
  EXPECT_EQ(profile1_submenu->GetLabelAt(0), u"Component Extension");
  EXPECT_FALSE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(0)));
  EXPECT_EQ(profile1_submenu->GetCommandIdAt(0), 0);
  EXPECT_EQ(profile1_submenu->GetLabelAt(1),
            u"Component Extension with Options");
  EXPECT_TRUE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(1)));
  EXPECT_EQ(profile1_submenu->GetCommandIdAt(1), 1);
  EXPECT_EQ(profile1_submenu->GetLabelAt(2), u"Regular Extension");
  EXPECT_TRUE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(2)));
  EXPECT_EQ(profile1_submenu->GetCommandIdAt(2), 2);
  EXPECT_EQ(profile1_submenu->GetLabelAt(3), u"Regular Extension with Options");
  EXPECT_TRUE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(3)));
  EXPECT_EQ(profile1_submenu->GetCommandIdAt(3), 3);

  // Profile 2 Submenu Checks
  StatusIconMenuModel* profile2_submenu =
      static_cast<StatusIconMenuModel*>(context_menu->GetSubmenuModelAt(4));
  EXPECT_EQ(profile2_submenu->GetLabelAt(0), u"Component Extension");
  EXPECT_FALSE(
      profile2_submenu->IsCommandIdEnabled(
          profile2_submenu->GetCommandIdAt(0)));
  EXPECT_EQ(profile2_submenu->GetCommandIdAt(0), 5);
  EXPECT_EQ(profile2_submenu->GetLabelAt(1), u"Regular Extension");
  EXPECT_TRUE(
      profile2_submenu->IsCommandIdEnabled(
          profile2_submenu->GetCommandIdAt(1)));
  EXPECT_EQ(profile2_submenu->GetCommandIdAt(1), 6);
  EXPECT_EQ(profile2_submenu->GetLabelAt(2), u"Regular Extension with Options");
  EXPECT_TRUE(
      profile2_submenu->IsCommandIdEnabled(
          profile2_submenu->GetCommandIdAt(2)));
  EXPECT_EQ(profile2_submenu->GetCommandIdAt(2), 7);

  // Model Adapter Checks for crbug.com/315164
  // P1: Profile 1 Menu Item
  // P2: Profile 2 Menu Item
  // CE: Component Extension Menu Item
  // CEO: Component Extenison with Options Menu Item
  // RE: Regular Extension Menu Item
  // REO: Regular Extension with Options Menu Item
  EXPECT_FALSE(IsCommandEnabled(context_menu, 0));  // P1 - CE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 1));   // P1 - CEO
  EXPECT_TRUE(IsCommandEnabled(context_menu, 2));   // P1 - RE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 3));   // P1 - REO
  EXPECT_TRUE(IsCommandEnabled(context_menu, 4));   // P1
  EXPECT_FALSE(IsCommandEnabled(context_menu, 5));  // P2 - CE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 6));   // P2 - RE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 7));   // P2 - REO
  EXPECT_TRUE(IsCommandEnabled(context_menu, 8));   // P2
}

TEST_F(BackgroundModeManagerWithExtensionsTest, BalloonDisplay) {
  scoped_refptr<const extensions::Extension> bg_ext =
      extensions::ExtensionBuilder("Background Extension")
          .SetVersion("1.0")
          .SetLocation(ManifestLocation::kCommandLine)
          .AddAPIPermission("background")
          .Build();

  scoped_refptr<const extensions::Extension> upgraded_bg_ext =
      extensions::ExtensionBuilder("Background Extension")
          .SetVersion("2.0")
          .SetLocation(ManifestLocation::kCommandLine)
          .AddAPIPermission("background")
          .Build();

  scoped_refptr<const extensions::Extension> no_bg_ext =
      extensions::ExtensionBuilder("Regular Extension")
          .SetVersion("1.0")
          .SetLocation(ManifestLocation::kCommandLine)
          .Build();

  scoped_refptr<const extensions::Extension> upgraded_no_bg_ext_has_bg =
      extensions::ExtensionBuilder("Regular Extension")
          .SetVersion("1.0")
          .SetLocation(ManifestLocation::kCommandLine)
          .AddAPIPermission("background")
          .Build();

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_))
      ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                               base::FilePath(), false);

  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_);
  ASSERT_FALSE(system->is_ready());
  extensions::ExtensionService* service = system->extension_service();
  service->Init();
  base::RunLoop run_loop;
  system->ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(system->is_ready());
  manager_->status_icon_ = new TestStatusIcon();
  manager_->UpdateStatusTrayIconContextMenu();

  // Adding a background extension should show the balloon.
  EXPECT_FALSE(manager_->HasShownBalloon());
  EXPECT_CALL(*manager_, EnableLaunchOnStartup(true)).Times(Exactly(1));
  service->AddExtension(bg_ext.get());
  Mock::VerifyAndClearExpectations(manager_.get());
  EXPECT_TRUE(manager_->HasShownBalloon());

  // Adding an extension without background should not show the balloon.
  manager_->SetHasShownBalloon(false);
  service->AddExtension(no_bg_ext.get());
  EXPECT_FALSE(manager_->HasShownBalloon());

  // Upgrading an extension that has background should not reshow the balloon.
  {
    // TODO(crbug.com/41145854): Fix crbug.com/438376 and remove these checks.
    InSequence expected_call_sequence;
    EXPECT_CALL(*manager_, EnableLaunchOnStartup(false)).Times(Exactly(1));
    EXPECT_CALL(*manager_, EnableLaunchOnStartup(true)).Times(Exactly(1));
  }
  service->AddExtension(upgraded_bg_ext.get());
  Mock::VerifyAndClearExpectations(manager_.get());
  EXPECT_FALSE(manager_->HasShownBalloon());

  // Upgrading an extension that didn't have background to one that does should
  // show the balloon.
  service->AddExtension(upgraded_no_bg_ext_has_bg.get());
  EXPECT_TRUE(manager_->HasShownBalloon());
}

TEST_F(BackgroundModeManagerTest, TransientBackgroundApp) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);
  ProfileAttributesEntry* entry =
      profile_manager_->profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile_->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->GetBackgroundStatus());

  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(1);
  manager.SetBackgroundClientCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);

  // Mimic transient app launch.
  EXPECT_CALL(manager, EnableLaunchOnStartup(_)).Times(0);
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.SetPersistentBackgroundClientCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);
  EXPECT_FALSE(entry->GetBackgroundStatus());

  manager.SuspendBackgroundMode();
  AssertBackgroundModeInactive(manager);
  EXPECT_FALSE(entry->GetBackgroundStatus());
  manager.ResumeBackgroundMode();

  // Mimic transient app shutdown.
  EXPECT_CALL(manager, EnableLaunchOnStartup(_)).Times(0);
  manager.SetBackgroundClientCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeInactive(manager);
  EXPECT_FALSE(entry->GetBackgroundStatus());
}

TEST_F(BackgroundModeManagerTest, TransientBackgroundAppWithPersistent) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);
  ProfileAttributesEntry* entry =
      profile_manager_->profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile_->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->GetBackgroundStatus());

  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(1);
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);
  EXPECT_TRUE(entry->GetBackgroundStatus());

  // Mimic transient app launch.
  EXPECT_CALL(manager, EnableLaunchOnStartup(_)).Times(0);
  manager.SetBackgroundClientCountForProfile(profile_, 2);
  manager.SetPersistentBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);
  EXPECT_TRUE(entry->GetBackgroundStatus());

  manager.SuspendBackgroundMode();
  AssertBackgroundModeInactive(manager);
  EXPECT_TRUE(entry->GetBackgroundStatus());
  manager.ResumeBackgroundMode();

  // Mimic transient app shutdown.
  EXPECT_CALL(manager, EnableLaunchOnStartup(_)).Times(0);
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);
  EXPECT_TRUE(entry->GetBackgroundStatus());
}

TEST_F(BackgroundModeManagerTest,
       BackgroundPersistentAppWhileTransientRunning) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);
  ProfileAttributesEntry* entry =
      profile_manager_->profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile_->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->GetBackgroundStatus());

  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Mimic transient app launch.
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(1);
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.SetPersistentBackgroundClientCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);
  EXPECT_FALSE(entry->GetBackgroundStatus());

  // Mimic persistent app install.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(1);
  manager.SetBackgroundClientCountForProfile(profile_, 2);
  manager.SetPersistentBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);
  EXPECT_TRUE(entry->GetBackgroundStatus());

  manager.SuspendBackgroundMode();
  AssertBackgroundModeInactive(manager);
  EXPECT_TRUE(entry->GetBackgroundStatus());
  manager.ResumeBackgroundMode();

  // Mimic persistent app uninstall.
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(1);
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.SetPersistentBackgroundClientCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);
  EXPECT_FALSE(entry->GetBackgroundStatus());
}

TEST_F(BackgroundModeManagerTest, ForceInstalledExtensionsKeepAlive) {
  const auto* keep_alive_registry = KeepAliveRegistry::GetInstance();
  EXPECT_FALSE(keep_alive_registry->IsKeepingAlive());

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kNoStartupWindow);
  TestBackgroundModeManager manager(
      command_line, profile_manager_->profile_attributes_storage());

  EXPECT_TRUE(keep_alive_registry->IsKeepingAlive());
  EXPECT_TRUE(keep_alive_registry->WouldRestartWithout({
      KeepAliveOrigin::BACKGROUND_MODE_MANAGER_STARTUP,
      KeepAliveOrigin::BACKGROUND_MODE_MANAGER_FORCE_INSTALLED_EXTENSIONS,
  }));

  manager.RegisterProfile(profile_);
  EXPECT_TRUE(keep_alive_registry->IsKeepingAlive());
  EXPECT_TRUE(keep_alive_registry->WouldRestartWithout({
      KeepAliveOrigin::BACKGROUND_MODE_MANAGER_STARTUP,
      KeepAliveOrigin::BACKGROUND_MODE_MANAGER_FORCE_INSTALLED_EXTENSIONS,
  }));

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_))
      ->CreateExtensionService(&command_line, base::FilePath(), false);
  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_))
      ->SetReady();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(keep_alive_registry->IsKeepingAlive());
  EXPECT_TRUE(keep_alive_registry->WouldRestartWithout({
      KeepAliveOrigin::BACKGROUND_MODE_MANAGER_FORCE_INSTALLED_EXTENSIONS,
  }));

  manager.GetBackgroundModeData(profile_)->OnForceInstalledExtensionsReady();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(keep_alive_registry->IsKeepingAlive());
}

TEST_F(BackgroundModeManagerTest,
       ForceInstalledExtensionsKeepAliveReleasedOnAppTerminating) {
  const auto* keep_alive_registry = KeepAliveRegistry::GetInstance();
  EXPECT_FALSE(keep_alive_registry->IsKeepingAlive());

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kNoStartupWindow);
  TestBackgroundModeManager manager(
      command_line, profile_manager_->profile_attributes_storage());

  manager.RegisterProfile(profile_);
  EXPECT_TRUE(keep_alive_registry->IsKeepingAlive());
  EXPECT_TRUE(keep_alive_registry->WouldRestartWithout({
      KeepAliveOrigin::BACKGROUND_MODE_MANAGER_STARTUP,
      KeepAliveOrigin::BACKGROUND_MODE_MANAGER_FORCE_INSTALLED_EXTENSIONS,
  }));

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_))
      ->CreateExtensionService(&command_line, base::FilePath(), false);
  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_))
      ->SetReady();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(keep_alive_registry->IsKeepingAlive());
  EXPECT_TRUE(keep_alive_registry->WouldRestartWithout({
      KeepAliveOrigin::BACKGROUND_MODE_MANAGER_FORCE_INSTALLED_EXTENSIONS,
  }));

  manager.OnAppTerminating();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(keep_alive_registry->IsKeepingAlive());
}
