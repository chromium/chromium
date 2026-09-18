// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_status_icon.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_file_util.h"
#include "base/version_info/channel.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background/glic/glic_background_mode_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace glic {
namespace {

int GetExpectedTooltipId(bool would_close) {
  if (would_close) {
    switch (chrome::GetChannel()) {
      case version_info::Channel::CANARY:
        return IDS_GLIC_STATUS_ICON_TOOLTIP_CLOSE_CANARY;
      case version_info::Channel::DEV:
        return IDS_GLIC_STATUS_ICON_TOOLTIP_CLOSE_DEV;
      case version_info::Channel::BETA:
        return IDS_GLIC_STATUS_ICON_TOOLTIP_CLOSE_BETA;
      default:
        return IDS_GLIC_STATUS_ICON_TOOLTIP_CLOSE;
    }
  }
  switch (chrome::GetChannel()) {
    case version_info::Channel::CANARY:
      return IDS_GLIC_STATUS_ICON_TOOLTIP_CANARY;
    case version_info::Channel::DEV:
      return IDS_GLIC_STATUS_ICON_TOOLTIP_DEV;
    case version_info::Channel::BETA:
      return IDS_GLIC_STATUS_ICON_TOOLTIP_BETA;
    default:
      return IDS_GLIC_STATUS_ICON_TOOLTIP;
  }
}
class MockStatusIcon : public StatusIcon {
 public:
  MockStatusIcon() = default;
  void SetImage(const gfx::ImageSkia& image) override {}
  void SetToolTip(const std::u16string& tool_tip) override {
    tool_tip_ = tool_tip;
  }
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {}
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override {}
  const std::u16string& tool_tip() const { return tool_tip_; }

 private:
  std::u16string tool_tip_;
};

class MockStatusTray : public StatusTray {
 public:
  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override {
    auto icon = std::make_unique<MockStatusIcon>();
    icon->SetToolTip(tool_tip);
    return icon;
  }

  const StatusIcons& GetStatusIconsForTesting() const { return status_icons(); }
};

class MockGlicBackgroundDelegate : public GlicBackgroundDelegate {
 public:
  MOCK_METHOD(void,
              ToggleUI,
              (bool prevent_close, mojom::InvocationSource source),
              (override));
  MOCK_METHOD(bool, WouldToggleClose, (), (const, override));
};

}  // namespace

// TODO(b/489122337): Fix this test.
class GlicStatusIconTest : public testing::Test {
 public:
  ~GlicStatusIconTest() override = default;

  void SetUp() override {
    ON_CALL(glic_background_delegate_mock_, WouldToggleClose())
        .WillByDefault(testing::ReturnPointee(&would_toggle_close_));

    glic_status_icon_ =
        GlicStatusIcon::Create(&glic_background_delegate_mock_, &status_tray_);
    glic_status_icon_->Init();
  }

  void TearDown() override { glic_status_icon_.reset(); }

  GlicStatusIcon* glic_status_icon() { return glic_status_icon_.get(); }
  MockGlicBackgroundDelegate* glic_background_delegate_mock() {
    return &glic_background_delegate_mock_;
  }
  MockStatusIcon* status_icon() {
    return static_cast<MockStatusIcon*>(
        status_tray_.GetStatusIconsForTesting().back().icon.get());
  }

  void SetWouldToggleCloseAndRefresh(bool would_close) {
    would_toggle_close_ = would_close;
    glic_status_icon_->RefreshToggleLabel();
  }

  void set_would_toggle_close(bool would_close) {
    would_toggle_close_ = would_close;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  GlicUnitTestEnvironment glic_test_env_;

  bool would_toggle_close_ = false;
  std::unique_ptr<GlicStatusIcon> glic_status_icon_;
  MockStatusTray status_tray_;
  testing::NiceMock<MockGlicBackgroundDelegate> glic_background_delegate_mock_;
  base::HistogramTester histogram_;
};

#if !BUILDFLAG(IS_LINUX)
TEST_F(GlicStatusIconTest, OnStatusIconClicked) {
  EXPECT_CALL(*glic_background_delegate_mock(), ToggleUI(false, testing::_))
      .Times(1);
  status_icon()->DispatchClickEvent();
}
#endif

TEST_F(GlicStatusIconTest, ExecuteCommand) {
  EXPECT_CALL(*glic_background_delegate_mock(), ToggleUI(false, testing::_))
      .Times(1);
  base::UserActionTester user_action_tester;
  auto* context_menu = status_icon()->GetContextMenuForTesting();
  context_menu->ExecuteCommand(IDC_GLIC_STATUS_ICON_MENU_TOGGLE, 0);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "GlicOsEntrypoint.ContextMenuSelection.ToggleGlic"));
}

TEST_F(GlicStatusIconTest, ContextMenu) {
  auto* context_menu = status_icon()->GetContextMenuForTesting();
  EXPECT_TRUE(
      context_menu->IsCommandIdVisible(IDC_GLIC_STATUS_ICON_MENU_TOGGLE));
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
  ui::Accelerator toggle_accelerator;
  EXPECT_TRUE(context_menu->GetAcceleratorForCommandId(
      IDC_GLIC_STATUS_ICON_MENU_TOGGLE, &toggle_accelerator));
  EXPECT_EQ(toggle_accelerator, new_accelerator);
}

TEST_F(GlicStatusIconTest, InitialStateWouldOpen) {
  EXPECT_FALSE(glic_status_icon()->would_close_for_testing());
  EXPECT_EQ(
      status_icon()->tool_tip(),
      l10n_util::GetStringUTF16(GetExpectedTooltipId(/*would_close=*/false)));
  auto* context_menu = status_icon()->GetContextMenuForTesting();
  std::optional<size_t> toggle_index =
      context_menu->GetIndexOfCommandId(IDC_GLIC_STATUS_ICON_MENU_TOGGLE);
  ASSERT_TRUE(toggle_index.has_value());
  EXPECT_EQ(context_menu->GetLabelAt(toggle_index.value()),
            l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_MENU_SHOW));
}

TEST_F(GlicStatusIconTest, RefreshToggleLabelUpdatesTooltipAndMenu) {
  auto* context_menu = status_icon()->GetContextMenuForTesting();
  std::optional<size_t> toggle_index =
      context_menu->GetIndexOfCommandId(IDC_GLIC_STATUS_ICON_MENU_TOGGLE);
  ASSERT_TRUE(toggle_index.has_value());

  SetWouldToggleCloseAndRefresh(true);
  EXPECT_TRUE(glic_status_icon()->would_close_for_testing());
  EXPECT_EQ(
      status_icon()->tool_tip(),
      l10n_util::GetStringUTF16(GetExpectedTooltipId(/*would_close=*/true)));
  EXPECT_EQ(context_menu->GetLabelAt(toggle_index.value()),
            l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_MENU_CLOSE));

  SetWouldToggleCloseAndRefresh(true);
  EXPECT_TRUE(glic_status_icon()->would_close_for_testing());

  SetWouldToggleCloseAndRefresh(false);
  EXPECT_FALSE(glic_status_icon()->would_close_for_testing());
  EXPECT_EQ(
      status_icon()->tool_tip(),
      l10n_util::GetStringUTF16(GetExpectedTooltipId(/*would_close=*/false)));
  EXPECT_EQ(context_menu->GetLabelAt(toggle_index.value()),
            l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_MENU_SHOW));
}

TEST_F(GlicStatusIconTest, InitialStateWouldClose) {
  testing::NiceMock<MockGlicBackgroundDelegate> close_delegate;
  ON_CALL(close_delegate, WouldToggleClose())
      .WillByDefault(testing::Return(true));
  MockStatusTray status_tray;
  std::unique_ptr<GlicStatusIcon> icon =
      GlicStatusIcon::Create(&close_delegate, &status_tray);
  icon->Init();

  EXPECT_TRUE(icon->would_close_for_testing());
  auto* mock_status_icon = static_cast<MockStatusIcon*>(
      status_tray.GetStatusIconsForTesting().back().icon.get());
  EXPECT_EQ(
      mock_status_icon->tool_tip(),
      l10n_util::GetStringUTF16(GetExpectedTooltipId(/*would_close=*/true)));
  auto* context_menu = mock_status_icon->GetContextMenuForTesting();
  std::optional<size_t> toggle_index =
      context_menu->GetIndexOfCommandId(IDC_GLIC_STATUS_ICON_MENU_TOGGLE);
  ASSERT_TRUE(toggle_index.has_value());
  EXPECT_EQ(context_menu->GetLabelAt(toggle_index.value()),
            l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_MENU_CLOSE));
}

TEST_F(GlicStatusIconTest, BrowserActivationRefreshesLabel) {
  ASSERT_FALSE(glic_status_icon()->would_close_for_testing());

  set_would_toggle_close(true);
  glic_status_icon()->OnBrowserActivated(nullptr);
  EXPECT_TRUE(glic_status_icon()->would_close_for_testing());

  set_would_toggle_close(false);
  glic_status_icon()->OnBrowserDeactivated(nullptr);
  EXPECT_FALSE(glic_status_icon()->would_close_for_testing());
}
}  // namespace glic
