// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_mode_manager.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_trigger.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
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
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#endif

using testing::_;
using testing::AtMost;
using testing::Exactly;
using testing::InSequence;
using testing::Mock;
using testing::StrictMock;

namespace {

std::unique_ptr<TestingProfileManager> CreateTestingProfileManager() {
  std::unique_ptr<TestingProfileManager> profile_manager(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  EXPECT_TRUE(profile_manager->SetUp());
  return profile_manager;
}

class FakeBackgroundTrigger : public BackgroundTrigger {
 public:
  ~FakeBackgroundTrigger() override;
  base::string16 GetName() override;
  gfx::ImageSkia* GetIcon() override;
  void OnMenuClick() override;
  int get_name_call_count_ = 0;
  int get_icon_call_count_ = 0;
  int on_menu_click_call_count_ = 0;
};

FakeBackgroundTrigger::~FakeBackgroundTrigger() {
}

base::string16 FakeBackgroundTrigger::GetName() {
  get_name_call_count_++;
  return base::ASCIIToUTF16("FakeBackgroundTrigger");
}

gfx::ImageSkia* FakeBackgroundTrigger::GetIcon() {
  get_icon_call_count_++;
  return nullptr;
}

void FakeBackgroundTrigger::OnMenuClick() {
  on_menu_click_call_count_++;
}

// Helper class that tracks state transitions in BackgroundModeManager and
// exposes them via getters (or gmock for EnableLaunchOnStartup).
class TestBackgroundModeManager : public StrictMock<BackgroundModeManager> {
 public:
  TestBackgroundModeManager(const base::CommandLine& command_line,
                            ProfileAttributesStorage* storage)
      : StrictMock<BackgroundModeManager>(command_line, storage),
        have_status_tray_(false),
        has_shown_balloon_(false) {
    ResumeBackgroundMode();
  }
  ~TestBackgroundModeManager() override {}

  MOCK_METHOD1(EnableLaunchOnStartup, void(bool should_launch));

  // TODO: Use strict-mocking rather than keeping state through overrides below.
  void DisplayClientInstalledNotification(const base::string16& name) override {
    has_shown_balloon_ = true;
  }
  void CreateStatusTrayIcon() override { have_status_tray_ = true; }
  void RemoveStatusTrayIcon() override { have_status_tray_ = false; }

  bool HaveStatusTray() const { return have_status_tray_; }
  bool HasShownBalloon() const { return has_shown_balloon_; }
  void SetHasShownBalloon(bool value) { has_shown_balloon_ = value; }

 private:
  // Flags to track whether we have a status tray/have shown the balloon.
  bool have_status_tray_;
  bool has_shown_balloon_;

  DISALLOW_COPY_AND_ASSIGN(TestBackgroundModeManager);
};

class TestStatusIcon : public StatusIcon {
 public:
  TestStatusIcon() {}
  void SetImage(const gfx::ImageSkia& image) override {}
  void SetToolTip(const base::string16& tool_tip) override {}
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const base::string16& title,
                      const base::string16& contents,
                      const message_center::NotifierId& notifier_id) override {}
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestStatusIcon);
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
  ~AdvancedTestBackgroundModeManager() override {}

  // TestBackgroundModeManager:
  bool HasBackgroundClient() const override {
    for (const auto& profile_count_pair : profile_app_counts_) {
      if (profile_count_pair.second > 0)
        return true;
    }
    return false;
  }
  bool HasBackgroundClientForProfile(const Profile* profile) const override {
    auto it = profile_app_counts_.find(profile);
    if (it == profile_app_counts_.end()) {
      ADD_FAILURE();
      return false;
    }
    return it->second > 0;
  }
  bool IsBackgroundModePrefEnabled() const override { return enabled_; }

  void SetBackgroundClientCountForProfile(const Profile* profile,
                                          size_t count) {
    profile_app_counts_[profile] = count;
  }
  void SetEnabled(bool enabled) {
    enabled_ = enabled;
    OnBackgroundModeEnabledPrefChanged();
  }

 private:
  bool enabled_;
  std::map<const Profile*, size_t> profile_app_counts_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedTestBackgroundModeManager);
};

class BackgroundModeManagerTest : public testing::Test {
 public:
  BackgroundModeManagerTest() {}
  ~BackgroundModeManagerTest() override {}

  void SetUp() override {
    command_line_.reset(new base::CommandLine(base::CommandLine::NO_PROGRAM));
    profile_manager_ = CreateTestingProfileManager();
    profile_ = profile_manager_->CreateTestingProfile("p1");
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<base::CommandLine> command_line_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Test profile used by all tests - this is owned by profile_manager_.
  TestingProfile* profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundModeManagerTest);
};

class BackgroundModeManagerWithExtensionsTest : public testing::Test {
 public:
  BackgroundModeManagerWithExtensionsTest() {}
  ~BackgroundModeManagerWithExtensionsTest() override {}

  void SetUp() override {
    command_line_.reset(new base::CommandLine(base::CommandLine::NO_PROGRAM));
    profile_manager_ = CreateTestingProfileManager();
    profile_ = profile_manager_->CreateTestingProfile("p1");

    test_keep_alive_.reset(
        new ScopedKeepAlive(KeepAliveOrigin::BACKGROUND_MODE_MANAGER,
                            KeepAliveRestartOption::DISABLED));

    // Create our test BackgroundModeManager.
    manager_.reset(new TestBackgroundModeManager(
        *command_line_, profile_manager_->profile_attributes_storage()));
    manager_->RegisterProfile(profile_);
  }

  void TearDown() override {
    // Clean up the status icon. If this is not done before profile deletes,
    // the context menu updates will DCHECK with the now deleted profiles.
    delete manager_->status_icon_;
    manager_->status_icon_ = nullptr;

    // We have to destroy the profiles now because we created them with real
    // thread state. This causes a lot of machinery to spin up that stops
    // working when we tear down our thread state at the end of the test.
    // Deleting our testing profile may have the side-effect of disabling
    // background mode if it was enabled for that profile (explicitly note that
    // here to satisfy StrictMock requirements.
    EXPECT_CALL(*manager_, EnableLaunchOnStartup(false)).Times(AtMost(1));
    profile_manager_->DeleteAllTestingProfiles();
    Mock::VerifyAndClearExpectations(manager_.get());

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
    int index = 0;
    if (ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index))
      return model->IsEnabledAt(index);

    return false;
  }

  std::unique_ptr<TestBackgroundModeManager> manager_;

  std::unique_ptr<base::CommandLine> command_line_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Test profile used by all tests - this is owned by profile_manager_.
  TestingProfile* profile_;

 private:
  // Required for extension service.
  content::TestBrowserThreadBundle thread_bundle_;

  // BackgroundModeManager actually affects Chrome start/stop state,
  // tearing down our thread bundle before we've had chance to clean
  // everything up. Keeping Chrome alive prevents this.
  // We aren't interested in if the keep alive works correctly in this test.
  std::unique_ptr<ScopedKeepAlive> test_keep_alive_;

#if defined(OS_CHROMEOS)
  // ChromeOS needs extra services to run in the following order.
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BackgroundModeManagerWithExtensionsTest);
};


TEST_F(BackgroundModeManagerTest, BackgroundAppLoadUnload) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage(), true);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Mimic app load.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundClientInstalled(base::ASCIIToUTF16("name"));
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
  manager.OnBackgroundClientInstalled(base::ASCIIToUTF16("name"));
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
  manager.OnBackgroundClientInstalled(base::ASCIIToUTF16("name"));
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

  // Turn off background mode (shouldn't explicitly disable launch-on-startup as
  // the app-count is zero and launch-on-startup shouldn't be considered on).
  manager.SetEnabled(false);
  AssertBackgroundModeInactive(manager);

  // When a new client is installed, status tray icons will not be created,
  // launch on startup status will not be modified.
  manager.OnBackgroundClientInstalled(base::ASCIIToUTF16("name"));
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
  manager.OnBackgroundClientInstalled(base::ASCIIToUTF16("name"));
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
  manager.OnBackgroundClientInstalled(base::ASCIIToUTF16("name"));
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);

  // Install app for other profile, should show other status tray icon.
  manager.OnBackgroundClientInstalled(base::ASCIIToUTF16("name"));
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

  ProfileAttributesEntry* entry1;
  ProfileAttributesEntry* entry2;
  ASSERT_TRUE(storage->GetProfileAttributesWithPath(profile_->GetPath(),
                                                    &entry1));
  ASSERT_TRUE(storage->GetProfileAttributesWithPath(profile2->GetPath(),
                                                    &entry2));

  EXPECT_FALSE(entry1->GetBackgroundStatus());
  EXPECT_FALSE(entry2->GetBackgroundStatus());

  // Install app, should show status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundClientInstalled(base::ASCIIToUTF16("name"));
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);

  // Install app for other profile.
  manager.OnBackgroundClientInstalled(base::ASCIIToUTF16("name"));
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
  manager.OnBackgroundClientInstalled(base::ASCIIToUTF16("name"));
  manager.SetBackgroundClientCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);

  // Background mode should remain active for the remainder of this test.

  manager.OnProfileNameChanged(
      profile_->GetPath(),
      manager.GetBackgroundModeData(profile_)->name());

  EXPECT_EQ(base::ASCIIToUTF16("p1"),
            manager.GetBackgroundModeData(profile_)->name());

  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  TestingProfile* profile2 = profile_manager_->CreateTestingProfile("p2");
  manager.RegisterProfile(profile2);
  EXPECT_EQ(2U, manager.NumberOfBackgroundModeData());

  manager.OnProfileAdded(profile2->GetPath());
  EXPECT_EQ(base::ASCIIToUTF16("p2"),
            manager.GetBackgroundModeData(profile2)->name());

  manager.OnProfileWillBeRemoved(profile2->GetPath());
  // Should still be in background mode after deleting profile.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  EXPECT_EQ(1U, manager.NumberOfBackgroundModeData());

  // Check that the background mode data we think is in the map actually is.
  EXPECT_EQ(base::ASCIIToUTF16("p1"),
            manager.GetBackgroundModeData(profile_)->name());
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
  manager.OnBackgroundClientInstalled(base::ASCIIToUTF16("name"));
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
          .SetLocation(extensions::Manifest::COMPONENT)
          .AddPermission("background")
          .Build();

  scoped_refptr<const extensions::Extension> component_extension_with_options =
      extensions::ExtensionBuilder("Component Extension with Options")
          .SetLocation(extensions::Manifest::COMPONENT)
          .AddPermission("background")
          .SetManifestKey("options_page", "test.html")
          .Build();

  scoped_refptr<const extensions::Extension> regular_extension =
      extensions::ExtensionBuilder("Regular Extension")
          .SetLocation(extensions::Manifest::COMMAND_LINE)
          .AddPermission("background")
          .Build();

  scoped_refptr<const extensions::Extension> regular_extension_with_options =
      extensions::ExtensionBuilder("Regular Extension with Options")
          .SetLocation(extensions::Manifest::COMMAND_LINE)
          .AddPermission("background")
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
  EXPECT_EQ(submenu->GetLabelAt(0), base::ASCIIToUTF16("Component Extension"));
  EXPECT_FALSE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(0)));
  EXPECT_EQ(submenu->GetLabelAt(1),
            base::ASCIIToUTF16("Component Extension with Options"));
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(1)));
  EXPECT_EQ(submenu->GetLabelAt(2), base::ASCIIToUTF16("Regular Extension"));
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(2)));
  EXPECT_EQ(submenu->GetLabelAt(3),
            base::ASCIIToUTF16("Regular Extension with Options"));
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(3)));
}

TEST_F(BackgroundModeManagerWithExtensionsTest,
       BackgroundMenuGenerationMultipleProfile) {
  scoped_refptr<const extensions::Extension> component_extension =
      extensions::ExtensionBuilder("Component Extension")
          .SetLocation(extensions::Manifest::COMPONENT)
          .AddPermission("background")
          .Build();

  scoped_refptr<const extensions::Extension> component_extension_with_options =
      extensions::ExtensionBuilder("Component Extension with Options")
          .SetLocation(extensions::Manifest::COMPONENT)
          .AddPermission("background")
          .SetManifestKey("options_page", "test.html")
          .Build();

  scoped_refptr<const extensions::Extension> regular_extension =
      extensions::ExtensionBuilder("Regular Extension")
          .SetLocation(extensions::Manifest::COMMAND_LINE)
          .AddPermission("background")
          .Build();

  scoped_refptr<const extensions::Extension> regular_extension_with_options =
      extensions::ExtensionBuilder("Regular Extension with Options")
          .SetLocation(extensions::Manifest::COMMAND_LINE)
          .AddPermission("background")
          .SetManifestKey("options_page", "test.html")
          .Build();

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_))
      ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                               base::FilePath(), false);
  extensions::ExtensionService* service1 =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service1->Init();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*manager_, EnableLaunchOnStartup(true)).Times(Exactly(1));
  service1->AddComponentExtension(component_extension.get());
  service1->AddComponentExtension(component_extension_with_options.get());
  service1->AddExtension(regular_extension.get());
  service1->AddExtension(regular_extension_with_options.get());
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

  service2->AddComponentExtension(component_extension.get());
  service2->AddExtension(regular_extension.get());
  service2->AddExtension(regular_extension_with_options.get());

  manager_->status_icon_ = new TestStatusIcon();
  manager_->UpdateStatusTrayIconContextMenu();
  StatusIconMenuModel* context_menu = manager_->context_menu_;
  EXPECT_TRUE(context_menu);

  // Background Profile Enable Checks
  EXPECT_EQ(context_menu->GetLabelAt(3), base::ASCIIToUTF16("p1"));
  EXPECT_TRUE(
      context_menu->IsCommandIdEnabled(context_menu->GetCommandIdAt(3)));
  EXPECT_EQ(context_menu->GetCommandIdAt(3), 4);

  EXPECT_EQ(context_menu->GetLabelAt(4), base::ASCIIToUTF16("p2"));
  EXPECT_TRUE(
      context_menu->IsCommandIdEnabled(context_menu->GetCommandIdAt(4)));
  EXPECT_EQ(context_menu->GetCommandIdAt(4), 8);

  // Profile 1 Submenu Checks
  StatusIconMenuModel* profile1_submenu =
      static_cast<StatusIconMenuModel*>(context_menu->GetSubmenuModelAt(3));
  EXPECT_EQ(profile1_submenu->GetLabelAt(0),
            base::ASCIIToUTF16("Component Extension"));
  EXPECT_FALSE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(0)));
  EXPECT_EQ(profile1_submenu->GetCommandIdAt(0), 0);
  EXPECT_EQ(profile1_submenu->GetLabelAt(1),
            base::ASCIIToUTF16("Component Extension with Options"));
  EXPECT_TRUE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(1)));
  EXPECT_EQ(profile1_submenu->GetCommandIdAt(1), 1);
  EXPECT_EQ(profile1_submenu->GetLabelAt(2),
            base::ASCIIToUTF16("Regular Extension"));
  EXPECT_TRUE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(2)));
  EXPECT_EQ(profile1_submenu->GetCommandIdAt(2), 2);
  EXPECT_EQ(profile1_submenu->GetLabelAt(3),
            base::ASCIIToUTF16("Regular Extension with Options"));
  EXPECT_TRUE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(3)));
  EXPECT_EQ(profile1_submenu->GetCommandIdAt(3), 3);

  // Profile 2 Submenu Checks
  StatusIconMenuModel* profile2_submenu =
      static_cast<StatusIconMenuModel*>(context_menu->GetSubmenuModelAt(4));
  EXPECT_EQ(profile2_submenu->GetLabelAt(0),
            base::ASCIIToUTF16("Component Extension"));
  EXPECT_FALSE(
      profile2_submenu->IsCommandIdEnabled(
          profile2_submenu->GetCommandIdAt(0)));
  EXPECT_EQ(profile2_submenu->GetCommandIdAt(0), 5);
  EXPECT_EQ(profile2_submenu->GetLabelAt(1),
            base::ASCIIToUTF16("Regular Extension"));
  EXPECT_TRUE(
      profile2_submenu->IsCommandIdEnabled(
          profile2_submenu->GetCommandIdAt(1)));
  EXPECT_EQ(profile2_submenu->GetCommandIdAt(1), 6);
  EXPECT_EQ(profile2_submenu->GetLabelAt(2),
            base::ASCIIToUTF16("Regular Extension with Options"));
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
  EXPECT_FALSE(IsCommandEnabled(context_menu, 0)); // P1 - CE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 1));  // P1 - CEO
  EXPECT_TRUE(IsCommandEnabled(context_menu, 2));  // P1 - RE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 3));  // P1 - REO
  EXPECT_TRUE(IsCommandEnabled(context_menu, 4));  // P1
  EXPECT_FALSE(IsCommandEnabled(context_menu, 5)); // P2 - CE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 6));  // P2 - RE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 7));  // P2 - REO
  EXPECT_TRUE(IsCommandEnabled(context_menu, 8));  // P2
}

TEST_F(BackgroundModeManagerWithExtensionsTest, BalloonDisplay) {
  scoped_refptr<const extensions::Extension> bg_ext =
      extensions::ExtensionBuilder("Background Extension")
          .SetVersion("1.0")
          .SetLocation(extensions::Manifest::COMMAND_LINE)
          .AddPermission("background")
          .Build();

  scoped_refptr<const extensions::Extension> upgraded_bg_ext =
      extensions::ExtensionBuilder("Background Extension")
          .SetVersion("2.0")
          .SetLocation(extensions::Manifest::COMMAND_LINE)
          .AddPermission("background")
          .Build();

  scoped_refptr<const extensions::Extension> no_bg_ext =
      extensions::ExtensionBuilder("Regular Extension")
          .SetVersion("1.0")
          .SetLocation(extensions::Manifest::COMMAND_LINE)
          .Build();

  scoped_refptr<const extensions::Extension> upgraded_no_bg_ext_has_bg =
      extensions::ExtensionBuilder("Regular Extension")
          .SetVersion("1.0")
          .SetLocation(extensions::Manifest::COMMAND_LINE)
          .AddPermission("background")
          .Build();

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_))
      ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                               base::FilePath(), false);

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  ASSERT_FALSE(service->is_ready());
  service->Init();
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(service->is_ready());
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
    // TODO: Fix crbug.com/438376 and remove these checks.
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

TEST_F(BackgroundModeManagerTest, TriggerRegisterUnregister) {
  FakeBackgroundTrigger trigger;
  TestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage());
  manager.RegisterProfile(profile_);
  AssertBackgroundModeInactive(manager);

  // Registering a trigger turns on background mode and shows a notification to
  // the user.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  manager.RegisterTrigger(profile_, &trigger, true /* should_notify_user */);
  Mock::VerifyAndClearExpectations(&manager);
  ASSERT_TRUE(manager.HasBackgroundClientForProfile(profile_));
  AssertBackgroundModeActive(manager);
  ASSERT_TRUE(manager.HasShownBalloon());

  // Unregistering the trigger turns off background mode.
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  manager.UnregisterTrigger(profile_, &trigger);
  Mock::VerifyAndClearExpectations(&manager);
  ASSERT_FALSE(manager.HasBackgroundClientForProfile(profile_));
  AssertBackgroundModeInactive(manager);
}

// TODO(mvanouwerkerk): Make background mode behavior consistent when
// registering a client while the pref is disabled - crbug.com/527032.
TEST_F(BackgroundModeManagerTest, TriggerRegisterWhileDisabled) {
  g_browser_process->local_state()->SetBoolean(prefs::kBackgroundModeEnabled,
                                               false);
  FakeBackgroundTrigger trigger;
  TestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_attributes_storage());
  manager.RegisterProfile(profile_);
  AssertBackgroundModeInactive(manager);
  ASSERT_FALSE(manager.IsBackgroundModePrefEnabled());

  // Registering a trigger while disabled has no immediate effect but it is
  // stored as pending in case background mode is later enabled.
  manager.RegisterTrigger(profile_, &trigger, true /* should_notify_user */);
  ASSERT_FALSE(manager.HasBackgroundClientForProfile(profile_));
  AssertBackgroundModeInactive(manager);
  ASSERT_FALSE(manager.HasShownBalloon());

  // When the background mode pref is enabled and there are pending triggers
  // they will be registered and the user will be notified.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  g_browser_process->local_state()->SetBoolean(prefs::kBackgroundModeEnabled,
                                               true);
  Mock::VerifyAndClearExpectations(&manager);
  ASSERT_TRUE(manager.HasBackgroundClientForProfile(profile_));
  AssertBackgroundModeActive(manager);
  ASSERT_TRUE(manager.HasShownBalloon());
}
