// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/omnibox_everywhere/omnibox_everywhere_background_mode_manager.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/message_center/public/cpp/notifier_id.h"

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
    TestingBrowserProcess::GetGlobal()->SetStatusTray(nullptr);
    ChromeViewsTestBase::TearDown();
  }

 protected:
  base::test::ScopedFeatureList feature_list_{omnibox::kOmniboxEverywhere};
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

  // Toggle background mode pref off and on.
  if (local_state) {
    local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, false);
    local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
  }
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
  EXPECT_EQ(accelerator, GetHotkey());
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

}  // namespace omnibox_everywhere
