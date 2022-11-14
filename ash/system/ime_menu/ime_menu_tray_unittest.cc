// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime_menu/ime_menu_tray.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/a11y_feature_type.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/ime/test_ime_controller_client.h"
#include "ash/public/cpp/ime_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/ime_menu/ime_list_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"

using base::UTF8ToUTF16;

namespace ash {
namespace {

const int kEmojiButtonId = 1;
const int kSettingsButtonId = 2;
const int kVoiceButtonId = 3;

ImeMenuTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->ime_menu_tray();
}

void SetCurrentIme(const std::string& current_ime_id,
                   const std::vector<ImeInfo>& available_imes) {
  std::vector<ImeInfo> available_ime_ptrs;
  for (const auto& ime : available_imes)
    available_ime_ptrs.push_back(ime);
  Shell::Get()->ime_controller()->RefreshIme(current_ime_id,
                                             std::move(available_ime_ptrs),
                                             std::vector<ImeMenuItem>());
}

}  // namespace

// Parameterized by feature QsRevamp.
class ImeMenuTrayTest : public AshTestBase,
                        public testing::WithParamInterface<bool> {
 public:
  ImeMenuTrayTest() = default;

  ImeMenuTrayTest(const ImeMenuTrayTest&) = delete;
  ImeMenuTrayTest& operator=(const ImeMenuTrayTest&) = delete;

  ~ImeMenuTrayTest() override = default;

 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kImeTrayHideVoiceButton};
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam()) {
      enabled_features.push_back(features::kQsRevamp);
      enabled_features.push_back(features::kQsRevampWip);
    } else {
      disabled_features.push_back(features::kQsRevamp);
      disabled_features.push_back(features::kQsRevampWip);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    AshTestBase::SetUp();
  }

  // Returns true if the IME menu tray is visible.
  bool IsVisible() { return GetTray()->GetVisible(); }

  // Returns the label text of the tray.
  const std::u16string& GetTrayText() { return GetTray()->label_->GetText(); }

  // Returns true if the background color of the tray is active.
  bool IsTrayBackgroundActive() { return GetTray()->is_active(); }

  // Returns true if the IME menu bubble has been shown.
  bool IsBubbleShown() { return GetTray()->GetBubbleView() != nullptr; }

  // Returns true if emoji palatte is enabled for the current keyboard.
  bool IsEmojiEnabled() { return GetTray()->is_emoji_enabled_; }

  // Returns true if handwirting input is enabled for the current keyboard.
  bool IsHandwritingEnabled() { return GetTray()->is_handwriting_enabled_; }

  // Returns true if voice input is enabled for the current keyboard.
  bool IsVoiceEnabled() { return GetTray()->is_voice_enabled_; }

  views::Button* GetEmojiButton() const {
    return static_cast<views::Button*>(
        GetTray()->bubble_->bubble_view()->GetViewByID(kEmojiButtonId));
  }

  views::View* GetSettingsButton() const {
    return static_cast<views::View*>(
        GetTray()->bubble_->bubble_view()->GetViewByID(kSettingsButtonId));
  }

  views::View* GetVoiceButton() const {
    return static_cast<views::View*>(
        GetTray()->bubble_->bubble_view()->GetViewByID(kVoiceButtonId));
  }

  void SetUpKioskSession() {
    SessionInfo info;
    info.is_running_in_app_mode = true;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }

  // Verifies the IME menu list has been updated with the right IME list.
  void ExpectValidImeList(const std::vector<ImeInfo>& expected_imes,
                          const ImeInfo& expected_current_ime) {
    const std::map<views::View*, std::string>& ime_map =
        ImeListViewTestApi(GetTray()->ime_list_view_).ime_map();
    EXPECT_EQ(expected_imes.size(), ime_map.size());

    std::vector<std::string> expected_ime_ids;
    for (const auto& ime : expected_imes) {
      expected_ime_ids.push_back(ime.id);
    }
    for (const auto& ime : ime_map) {
      // Tests that all the IMEs on the view is in the list of selected IMEs.
      EXPECT_TRUE(base::Contains(expected_ime_ids, ime.second));

      // Tests that the checked IME is the current IME.
      ui::AXNodeData node_data;
      ime.first->GetAccessibleNodeData(&node_data);
      const auto checked_state = static_cast<ax::mojom::CheckedState>(
          node_data.GetIntAttribute(ax::mojom::IntAttribute::kCheckedState));
      if (checked_state == ax::mojom::CheckedState::kTrue)
        EXPECT_EQ(expected_current_ime.id, ime.second);
    }
  }

  // Focuses in the given type of input context.
  void FocusInInputContext(ui::TextInputType input_type) {
    ui::IMEBridge::Get()->SetCurrentInputContext(
        ui::TextInputMethod::InputContext(input_type));
  }

  bool MenuHasOnScreenKeyboardToggle() const {
    if (!GetTray()->ime_list_view_)
      return false;
    return ImeListViewTestApi(GetTray()->ime_list_view_).GetToggleView();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp, ImeMenuTrayTest, testing::Bool());

// Tests that visibility of IME menu tray should be consistent with the
// activation of the IME menu.
TEST_P(ImeMenuTrayTest, ImeMenuTrayVisibility) {
  ASSERT_FALSE(IsVisible());

  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  EXPECT_TRUE(IsVisible());

  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(false);
  EXPECT_FALSE(IsVisible());
}

// Tests that IME menu tray shows the right info of the current IME.
TEST_P(ImeMenuTrayTest, TrayLabelTest) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());

  ImeInfo info1;
  info1.id = "ime1";
  info1.name = u"English";
  info1.short_name = u"US";
  info1.third_party = false;

  ImeInfo info2;
  info2.id = "ime2";
  info2.name = u"English UK";
  info2.short_name = u"UK";
  info2.third_party = true;

  // Changes the input method to "ime1".
  SetCurrentIme("ime1", {info1, info2});
  EXPECT_EQ(u"US", GetTrayText());

  // Changes the input method to a third-party IME extension.
  SetCurrentIme("ime2", {info1, info2});
  EXPECT_EQ(u"UK*", GetTrayText());
}

TEST_P(ImeMenuTrayTest, TrayLabelExludesDictation) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());

  ImeInfo info1;
  info1.id = "ime1";
  info1.name = u"English";
  info1.short_name = u"US";
  info1.third_party = false;

  ImeInfo info2;
  info2.id = "ime2";
  info2.name = u"English UK";
  info2.short_name = u"UK";
  info2.third_party = true;

  ImeInfo dictation;
  dictation.id = "_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation";
  dictation.name = u"Dictation";

  // Changes the input method to "ime1".
  SetCurrentIme("ime1", {info1, dictation, info2});
  EXPECT_EQ(u"US", GetTrayText());

  // Changes the input method to a third-party IME extension.
  SetCurrentIme("ime2", {info1, dictation, info2});
  EXPECT_EQ(u"UK*", GetTrayText());

  // Sets to "dictation", which shouldn't be shown.
  SetCurrentIme(dictation.id, {info1, dictation, info2});
  EXPECT_EQ(u"", GetTrayText());
}

// Tests that the IME menu tray changes background color when tapped, and
// tests that the background color becomes 'inactive' when disabling the IME
// menu feature. Also makes sure that the shelf won't autohide as long as the
// IME menu is open.
TEST_P(ImeMenuTrayTest, PerformActionGestureTap) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_FALSE(status->ShouldShowShelf());

  GestureTapOn(GetTray());
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  // Auto-hidden shelf would be forced to be visible as long as the bubble is
  // open.
  EXPECT_TRUE(status->ShouldShowShelf());

  GestureTapOn(GetTray());
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(IsBubbleShown());

  // If disabling the IME menu feature when the menu tray is activated, the tray
  // element will be deactivated.
  GestureTapOn(GetTray());
  EXPECT_TRUE(IsTrayBackgroundActive());
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(false);
  EXPECT_FALSE(IsVisible());
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(status->ShouldShowShelf());
}

// Tests that the IME menu reacts to left click.
TEST_P(ImeMenuTrayTest, PerformActionLeftClick) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  LeftClickOn(GetTray());
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  LeftClickOn(GetTray());
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(IsBubbleShown());
}

// Tests that IME menu list updates when changing the current IME. This should
// only happen by using shortcuts (Ctrl + Space / Ctrl + Shift + Space) to
// switch IMEs.
TEST_P(ImeMenuTrayTest, RefreshImeWithListViewCreated) {
  GetTray()->SetVisiblePreferred(true);
  GestureTapOn(GetTray());

  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  ImeInfo info1, info2, info3;
  info1.id = "ime1";
  info1.name = u"English";
  info1.short_name = u"US";
  info1.third_party = false;

  info2.id = "ime2";
  info2.name = u"English UK";
  info2.short_name = u"UK";
  info2.third_party = true;

  info3.id = "ime3";
  info3.name = u"Pinyin";
  info3.short_name = u"拼";
  info3.third_party = false;

  std::vector<ImeInfo> ime_info_list{info1, info2, info3};

  // Switch to ime1.
  SetCurrentIme("ime1", ime_info_list);
  EXPECT_EQ(u"US", GetTrayText());
  ExpectValidImeList(ime_info_list, info1);

  // Switch to ime3.
  SetCurrentIme("ime3", ime_info_list);
  EXPECT_EQ(u"拼", GetTrayText());
  ExpectValidImeList(ime_info_list, info3);

  // Closes the menu before quitting.
  GestureTapOn(GetTray());
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(IsBubbleShown());
}

// Tests that quits Chrome with IME menu openned will not crash.
TEST_P(ImeMenuTrayTest, QuitChromeWithMenuOpen) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  GestureTapOn(GetTray());
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());
}

// Tests using 'Alt+Shift+K' to open the menu.
TEST_P(ImeMenuTrayTest, TestAccelerator) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      TOGGLE_IME_MENU_BUBBLE, {});
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  GestureTapOn(GetTray());
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(IsBubbleShown());
}

TEST_P(ImeMenuTrayTest, ShowingEmojiKeysetHidesBubble) {
  // Setup the callback required by ui::ShowEmojiPanel() to a dummy one.
  // The ui::ShowEmojiPanel() call in ShowKeyboardWithKeyset will fail
  // without this callback.
  ui::SetShowEmojiKeyboardCallback(base::DoNothing());

  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  GestureTapOn(GetTray());
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  TestImeControllerClient client;
  Shell::Get()->ime_controller()->SetClient(&client);
  GetTray()->ShowKeyboardWithKeyset(input_method::ImeKeyset::kEmoji);

  // The menu should be hidden.
  EXPECT_FALSE(IsBubbleShown());
}

// Tests that the IME menu accelerator toggles the bubble on and off.
TEST_P(ImeMenuTrayTest, ImeBubbleAccelerator) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());
  EXPECT_FALSE(IsBubbleShown());

  PressAndReleaseKey(ui::VKEY_K, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBubbleShown());

  PressAndReleaseKey(ui::VKEY_K, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsBubbleShown());
}

// Tests that tapping the emoji button does not crash. http://crbug.com/739630
TEST_P(ImeMenuTrayTest, TapEmojiButton) {
  int call_count = 0;
  ui::SetShowEmojiKeyboardCallback(
      base::BindRepeating([](int* count) { (*count)++; }, (&call_count)));

  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  Shell::Get()->ime_controller()->SetExtraInputOptionsEnabledState(
      true /* ui enabled */, true /* emoji input enabled */,
      true /* hanwriting input enabled */, true /* voice input enabled */);

  // Open the menu.
  GestureTapOn(GetTray());

  // Tap the emoji button.
  views::Button* emoji_button = GetEmojiButton();
  ASSERT_TRUE(emoji_button);
  GestureTapOn(emoji_button);

  // The menu should be hidden.
  EXPECT_FALSE(IsBubbleShown());

  // The callback should have been called.
  EXPECT_EQ(call_count, 1);
}

TEST_P(ImeMenuTrayTest, ShouldShowBottomButtons) {
  Shell::Get()->ime_controller()->SetExtraInputOptionsEnabledState(
      true /* ui enabled */, true /* emoji input enabled */,
      true /* hanwriting input enabled */, true /* voice input enabled */);

  FocusInInputContext(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_TRUE(GetTray()->ShouldShowBottomButtons());
  EXPECT_TRUE(IsEmojiEnabled());
  EXPECT_TRUE(IsHandwritingEnabled());
  EXPECT_TRUE(IsVoiceEnabled());

  FocusInInputContext(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_FALSE(GetTray()->ShouldShowBottomButtons());
  EXPECT_FALSE(IsEmojiEnabled());
  EXPECT_FALSE(IsHandwritingEnabled());
  EXPECT_FALSE(IsVoiceEnabled());
}

TEST_P(ImeMenuTrayTest, ShouldShowBottomButtonsSeperate) {
  FocusInInputContext(ui::TEXT_INPUT_TYPE_TEXT);

  // Sets emoji disabled.
  Shell::Get()->ime_controller()->SetExtraInputOptionsEnabledState(
      true /* ui enabled */, false /* emoji input disabled */,
      true /* hanwriting input enabled */, true /* voice input enabled */);

  EXPECT_TRUE(GetTray()->ShouldShowBottomButtons());
  EXPECT_FALSE(IsEmojiEnabled());
  EXPECT_TRUE(IsHandwritingEnabled());
  EXPECT_TRUE(IsVoiceEnabled());

  // Sets emoji enabled, but voice and handwriting disabled.
  Shell::Get()->ime_controller()->SetExtraInputOptionsEnabledState(
      true /* ui enabled */, true /* emoji input enabled */,
      false /* hanwriting input disabled */, false /* voice input disabled */);

  EXPECT_TRUE(GetTray()->ShouldShowBottomButtons());
  EXPECT_TRUE(IsEmojiEnabled());
  EXPECT_FALSE(IsHandwritingEnabled());
  EXPECT_FALSE(IsVoiceEnabled());
}

TEST_P(ImeMenuTrayTest, KioskImeTraySettingsButton) {
  SetUpKioskSession();
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());

  // Open the menu.
  GestureTapOn(GetTray());

  views::View* settings_button = GetSettingsButton();
  EXPECT_FALSE(settings_button);
}

TEST_P(ImeMenuTrayTest, UserSessionImeTraySettingsButton) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());

  // Open the menu.
  GestureTapOn(GetTray());

  views::View* settings_button = GetSettingsButton();
  EXPECT_TRUE(settings_button);
}

TEST_P(ImeMenuTrayTest, ShowOnScreenKeyboardToggle) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  GestureTapOn(GetTray());
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  EXPECT_FALSE(MenuHasOnScreenKeyboardToggle());

  // The on-screen keyboard toggle should show if the device has a touch
  // screen, and does not have an internal keyboard.
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(screens);

  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_USB, "external keyboard"));
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);

  // Bubble gets closed when the keyboard suppression state changes.
  EXPECT_FALSE(IsBubbleShown());

  GestureTapOn(GetTray());
  EXPECT_TRUE(IsBubbleShown());

  EXPECT_TRUE(MenuHasOnScreenKeyboardToggle());

  // The toggle should not be removed on IME device refresh.
  ImeInfo info;
  info.id = "ime";
  info.name = u"English UK";
  info.short_name = u"UK";
  info.third_party = true;

  SetCurrentIme("ime", {info});
  EXPECT_TRUE(MenuHasOnScreenKeyboardToggle());

  // The toggle should be hidden with internal keyboard.
  keyboard_devices.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_USB, "external keyboard"));
  keyboard_devices.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "internal keyboard"));
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);

  // Bubble gets closed when the keyboard suppression state changes.
  EXPECT_FALSE(IsBubbleShown());

  GestureTapOn(GetTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_FALSE(MenuHasOnScreenKeyboardToggle());
}

TEST_P(ImeMenuTrayTest, ShowVoiceButtonWhenDictationDisabled) {
  // Enable all extra input options.
  Shell::Get()->ime_controller()->SetExtraInputOptionsEnabledState(
      /*is_extra_input_options_enabled=*/true,
      /*is_emoji_enabled=*/true,
      /*is_handwriting_enabled=*/true, /*is_voice_enabled=*/true);

  // Disable accessibility dictation.
  Shell::Get()
      ->accessibility_controller()
      ->GetFeature(A11yFeatureType::kDictation)
      .SetEnabled(false);

  // Show IME tray bubble.
  GetTray()->ShowBubble();

  // Voice button should be shown.
  views::View* voice_button = GetVoiceButton();
  EXPECT_TRUE(voice_button);
}

TEST_P(ImeMenuTrayTest, HideVoiceButtonWhenDictationEnabled) {
  // Enable all extra input options.
  Shell::Get()->ime_controller()->SetExtraInputOptionsEnabledState(
      /*is_extra_input_options_enabled=*/true,
      /*is_emoji_enabled=*/true,
      /*is_handwriting_enabled=*/true, /*is_voice_enabled=*/true);

  // Enable accessibility dictation.
  Shell::Get()
      ->accessibility_controller()
      ->GetFeature(A11yFeatureType::kDictation)
      .SetEnabled(true);

  // Show IME tray bubble.
  GetTray()->ShowBubble();

  // Voice button should be hidden.
  views::View* voice_button = GetVoiceButton();
  EXPECT_FALSE(voice_button);
}

}  // namespace ash
