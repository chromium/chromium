// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/omnibox_everywhere/omnibox_everywhere_background_mode_manager.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if BUILDFLAG(IS_WIN)
#include <optional>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/startup/startup_features.h"
#include "chrome/browser/startup/startup_launch_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#endif

namespace omnibox_everywhere {

namespace {

class MockStatusIcon : public StatusIcon {
 public:
  MockStatusIcon() = default;
  ~MockStatusIcon() override = default;

  void SetImage(const gfx::ImageSkia& image) override {}
  void SetToolTip(const std::u16string& tool_tip) override {}
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override {}
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {}
};

class MockStatusTray : public StatusTray {
 public:
  MockStatusTray() = default;
  ~MockStatusTray() override = default;

  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override {
    return std::make_unique<MockStatusIcon>();
  }
};

#if BUILDFLAG(IS_WIN)
class TestStartupLaunchManager : public StartupLaunchManager {
 public:
  explicit TestStartupLaunchManager(BrowserProcess* browser_process)
      : StartupLaunchManager(browser_process) {}

  MOCK_METHOD1(
      UpdateLaunchOnStartup,
      void(std::optional<auto_launch_util::StartupLaunchMode> startup_mode));
};
#endif

}  // namespace

class OmniboxEverywhereBackgroundModeManagerTest : public ChromeViewsTestBase {
 public:
  OmniboxEverywhereBackgroundModeManagerTest() = default;
  ~OmniboxEverywhereBackgroundModeManagerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    TestingBrowserProcess::GetGlobal()->SetStatusTray(
        std::make_unique<MockStatusTray>());
    if (PrefService* local_state =
            TestingBrowserProcess::GetGlobal()->local_state()) {
      local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
    }
  }

  void TearDown() override {
    profile_.reset();
    TestingBrowserProcess::GetGlobal()->SetStatusTray(nullptr);
    ChromeViewsTestBase::TearDown();
  }

#if BUILDFLAG(IS_WIN)
  TestStartupLaunchManager* startup_launch_manager() {
    return static_cast<TestStartupLaunchManager*>(
        StartupLaunchManager::From(g_browser_process));
  }

  void ExpectStartupRegistration(bool launch_enabled) {
    std::optional<auto_launch_util::StartupLaunchMode> launch_mode;
    if (launch_enabled) {
      launch_mode = auto_launch_util::StartupLaunchMode::kBackground;
    }
    EXPECT_CALL(*startup_launch_manager(), UpdateLaunchOnStartup(launch_mode))
        .Times(testing::AtLeast(1));
  }

  void VerifyAndClearStartupRegistrationExpectations() {
    testing::Mock::VerifyAndClearExpectations(startup_launch_manager());
  }
#endif

  TestingProfile* profile() {
    if (!profile_) {
      profile_ = std::make_unique<TestingProfile>();
    }
    return profile_.get();
  }

 protected:
  base::test::ScopedFeatureList feature_list_{omnibox::kOmniboxEverywhere};
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(OmniboxEverywhereBackgroundModeManagerTest, InitializationDoesNotCrash) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest, BackgroundModePrefToggle) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();

  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
  manager.SetProfile(profile());

  // Toggle background mode pref off and on.
  if (local_state) {
    local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, false);
    EXPECT_EQ(manager.status_icon_for_testing(), nullptr);
    local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
    EXPECT_NE(manager.status_icon_for_testing(), nullptr);
  }
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       RequiresProfileToEnableBackgroundMode) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  // Background mode is not entered without a profile even if the pref is
  // enabled.
  EXPECT_EQ(manager.status_icon_for_testing(), nullptr);

  // Background mode is entered and creates a status icon once a profile is set.
  manager.SetProfile(profile());
  EXPECT_NE(manager.status_icon_for_testing(), nullptr);

  // Clearing the profile resets background mode and removes the status icon.
  manager.SetProfile(nullptr);
  EXPECT_EQ(manager.status_icon_for_testing(), nullptr);

  // Setting the profile again re-enters background mode and restores the status
  // icon.
  manager.SetProfile(profile());
  EXPECT_NE(manager.status_icon_for_testing(), nullptr);
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       StatusIconClickTriggersCallback) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  // Simulate status icon click callback execution
  static_cast<StatusIconObserver*>(&manager)->OnStatusIconClicked();
  EXPECT_TRUE(callback_called);
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest, ContextMenuStructure) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
  manager.SetProfile(profile());

  StatusIcon* status_icon = manager.status_icon_for_testing();
  ASSERT_NE(status_icon, nullptr);

  StatusIconMenuModel* menu = status_icon->GetContextMenuForTesting();
  ASSERT_NE(menu, nullptr);
  ASSERT_EQ(menu->GetItemCount(), 3u);

  EXPECT_EQ(menu->GetCommandIdAt(0),
            IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE);
  EXPECT_EQ(
      menu->GetCommandIdAt(1),
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT);
  EXPECT_EQ(menu->GetCommandIdAt(2),
            IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_SETTINGS);

  ui::Accelerator accelerator;
  EXPECT_TRUE(menu->GetAcceleratorForCommandId(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, &accelerator));
  EXPECT_EQ(accelerator, prefs::GetDefaultOmniboxEverywhereHotkey());
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       ContextMenuUpdatesOnCustomHotkeyChange) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
  manager.SetProfile(profile());

  StatusIcon* status_icon = manager.status_icon_for_testing();
  ASSERT_NE(status_icon, nullptr);

  StatusIconMenuModel* menu = status_icon->GetContextMenuForTesting();
  ASSERT_NE(menu, nullptr);

  ui::Accelerator accelerator;
  EXPECT_TRUE(menu->GetAcceleratorForCommandId(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, &accelerator));
  EXPECT_EQ(accelerator, prefs::GetDefaultOmniboxEverywhereHotkey());

  // Update custom hotkey pref and verify the status icon context menu updates.
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetString(
      prefs::kOmniboxEverywhereHotkey, "Ctrl+Shift+Space");

  menu = status_icon->GetContextMenuForTesting();
  ASSERT_NE(menu, nullptr);
  EXPECT_TRUE(menu->GetAcceleratorForCommandId(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, &accelerator));
  EXPECT_EQ(
      accelerator,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN));
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest, ExecuteToggleCommand) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  StatusIconMenuModel::Delegate* delegate =
      static_cast<StatusIconMenuModel::Delegate*>(&manager);
  delegate->ExecuteCommand(IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, 0);
  EXPECT_TRUE(callback_called);
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       LaunchOnStartupPrefToggleWithoutStartupLaunchManagerDoesNotCrash) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  ASSERT_NE(local_state, nullptr);

  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  // Toggling launch on startup pref should not crash even when
  // StartupLaunchManager is not initialized in test environment.
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, true);
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, false);
}

#if BUILDFLAG(IS_WIN)
TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       LaunchOnStartupRegistrationWithStartupLaunchManager) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {omnibox::kOmniboxEverywhere},
      {features::kLaunchOnStartup, features::kLaunchOnStartupInfoBar});

  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  ASSERT_NE(local_state, nullptr);

  auto scoped_override =
      GlobalFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
          base::BindRepeating([](BrowserProcess& browser_process) {
            return std::make_unique<TestStartupLaunchManager>(&browser_process);
          }));

  // Reset any profile before replacing GlobalFeatures so keyed services (such
  // as NtpBackgroundService) do not retain dangling raw_refs to
  // GlobalFeatures components like ApplicationLocaleStorage.
  profile_.reset();
  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);

  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, false);

  // Consume any initial startup registration calls during setup.
  EXPECT_CALL(*startup_launch_manager(), UpdateLaunchOnStartup(testing::_))
      .Times(testing::AnyNumber());
  startup_launch_manager()->CommitLaunchOnStartupState();

  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
  VerifyAndClearStartupRegistrationExpectations();

  // 1. Enabling launch on startup registers background launch.
  ExpectStartupRegistration(/*launch_enabled=*/true);
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, true);
  VerifyAndClearStartupRegistrationExpectations();

  // 2. Disabling launch on startup unregisters background launch.
  ExpectStartupRegistration(/*launch_enabled=*/false);
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, false);
  VerifyAndClearStartupRegistrationExpectations();

  // 3. Re-enabling launch on startup registers background launch again.
  ExpectStartupRegistration(/*launch_enabled=*/true);
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, true);
  VerifyAndClearStartupRegistrationExpectations();

  // 4. Disabling background mode unregisters startup launch even if
  // launch_on_startup pref is true.
  ExpectStartupRegistration(/*launch_enabled=*/false);
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, false);
  VerifyAndClearStartupRegistrationExpectations();

  profile_.reset();
  TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
}
#endif

}  // namespace omnibox_everywhere
