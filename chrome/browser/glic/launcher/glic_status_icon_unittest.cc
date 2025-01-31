// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_status_icon.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/glic/launcher/glic_controller.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/common/chrome_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace glic {
namespace {
class MockStatusIcon : public StatusIcon {
 public:
  MockStatusIcon() = default;
  void SetImage(const gfx::ImageSkia& image) override {}
  void SetToolTip(const std::u16string& tool_tip) override {}
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {}
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override {}
};

class MockStatusTray : public StatusTray {
 public:
  std::unique_ptr<StatusIcon> CreateStatusIcon(StatusIconType type,
                                               const gfx::ImageSkia& image,
                                               const std::u16string& tool_tip) {
    return std::make_unique<MockStatusIcon>();
  }

  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override {
    return std::make_unique<MockStatusIcon>();
  }

  const StatusIcons& GetStatusIconsForTesting() const { return status_icons(); }
};

class MockGlicController : public GlicController {
 public:
  MOCK_METHOD0(Show, void());
};

}  // namespace

class GlicStatusIconTest : public testing::Test {
 public:
  GlicStatusIconTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }
  ~GlicStatusIconTest() override = default;

  void SetUp() override {
    glic_status_icon_ =
        std::make_unique<GlicStatusIcon>(&glic_controller_, &status_tray_);
  }

  void TearDown() override { glic_status_icon_.reset(); }

  GlicStatusIcon* glic_status_icon() { return glic_status_icon_.get(); }
  MockGlicController* glic_controller() { return &glic_controller_; }
  MockStatusIcon* status_icon() {
    return static_cast<MockStatusIcon*>(
        status_tray_.GetStatusIconsForTesting().back().icon.get());
  }
  base::HistogramTester* histogram() { return &histogram_; }

 private:
  std::unique_ptr<GlicStatusIcon> glic_status_icon_;
  MockStatusTray status_tray_;
  MockGlicController glic_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_;
};

TEST_F(GlicStatusIconTest, OnStatusIconClicked) {
  EXPECT_CALL(*glic_controller(), Show).Times(1);
  status_icon()->DispatchClickEvent();
}

TEST_F(GlicStatusIconTest, ExecuteCommand) {
  EXPECT_CALL(*glic_controller(), Show).Times(1);
  base::UserActionTester user_action_tester;
  auto* context_menu = status_icon()->GetContextMenuForTesting();
  context_menu->ExecuteCommand(IDC_GLIC_STATUS_ICON_MENU_SHOW, 0);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "GlicOsEntrypoint.ContextMenuSelection.OpenGlic"));
}

TEST_F(GlicStatusIconTest, ContextMenu) {
  auto* context_menu = status_icon()->GetContextMenuForTesting();
  EXPECT_TRUE(context_menu->IsCommandIdVisible(IDC_GLIC_STATUS_ICON_MENU_SHOW));
  EXPECT_TRUE(context_menu->IsCommandIdVisible(
      IDC_GLIC_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT));
  EXPECT_TRUE(
      context_menu->IsCommandIdVisible(IDC_GLIC_STATUS_ICON_MENU_REMOVE_ICON));
  EXPECT_TRUE(
      context_menu->IsCommandIdVisible(IDC_GLIC_STATUS_ICON_MENU_SETTINGS));
}

TEST_F(GlicStatusIconTest, UpdateHotkey) {
  auto* context_menu = status_icon()->GetContextMenuForTesting();
  ui::Accelerator new_accelerator(ui::VKEY_A,
                                  ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  glic_status_icon()->UpdateHotkey(new_accelerator);
  ui::Accelerator show_accelerator;
  EXPECT_TRUE(context_menu->GetAcceleratorForCommandId(
      IDC_GLIC_STATUS_ICON_MENU_SHOW, &show_accelerator));
  EXPECT_EQ(show_accelerator, new_accelerator);
}
}  // namespace glic
