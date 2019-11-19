// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime_menu/ime_menu_tray.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/ime/ime_controller.h"
#include "ash/ime/test_ime_controller_client.h"
#include "ash/public/mojom/ime_info.mojom.h"
#include "ash/shell.h"
#include "ash/system/ime_menu/ime_list_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"

using base::UTF8ToUTF16;

namespace ash {
namespace {

const int kEmojiButtonId = 1;

ImeMenuTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->ime_menu_tray();
}

void SetCurrentIme(const std::string& current_ime_id,
                   const std::vector<mojom::ImeInfo>& available_imes) {
  std::vector<mojom::ImeInfoPtr> available_ime_ptrs;
  for (const auto& ime : available_imes)
    available_ime_ptrs.push_back(ime.Clone());
  Shell::Get()->ime_controller()->RefreshIme(
      current_ime_id, std::move(available_ime_ptrs),
      std::vector<mojom::ImeMenuItemPtr>());
}

}  // namespace

class ImeMenuTrayTest : public AshTestBase {
 public:
  ImeMenuTrayTest() = default;
  ~ImeMenuTrayTest() override = default;

 protected:
  // Returns true if the IME menu tray is visible.
  bool IsVisible() { return GetTray()->GetVisible(); }

  // Returns the label text of the tray.
  const base::string16& GetTrayText() { return GetTray()->label_->GetText(); }

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

  // Verifies the IME menu list has been updated with the right IME list.
  void ExpectValidImeList(const std::vector<mojom::ImeInfo>& expected_imes,
                          const mojom::ImeInfo& expected_current_ime) {
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
    ui::IMEEngineHandlerInterface::InputContext input_context(
        input_type, ui::TEXT_INPUT_MODE_DEFAULT, ui::TEXT_INPUT_FLAG_NONE,
        ui::TextInputClient::FOCUS_REASON_OTHER,
        false /* should_do_learning */);
    ui::IMEBridge::Get()->SetCurrentInputContext(input_context);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImeMenuTrayTest);
};

// Tests that visibility of IME menu tray should be consistent with the
// activation of the IME menu.
TEST_F(ImeMenuTrayTest, ImeMenuTrayVisibility) {
  ASSERT_FALSE(IsVisible());

  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  EXPECT_TRUE(IsVisible());

  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(false);
  EXPECT_FALSE(IsVisible());
}

// Tests that IME menu tray shows the right info of the current IME.
TEST_F(ImeMenuTrayTest, TrayLabelTest) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());

  mojom::ImeInfo info1;
  info1.id = "ime1";
  info1.name = UTF8ToUTF16("English");
  info1.medium_name = UTF8ToUTF16("English");
  info1.short_name = UTF8ToUTF16("US");
  info1.third_party = false;

  mojom::ImeInfo info2;
  info2.id = "ime2";
  info2.name = UTF8ToUTF16("English UK");
  info2.medium_name = UTF8ToUTF16("English UK");
  info2.short_name = UTF8ToUTF16("UK");
  info2.third_party = true;

  // Changes the input method to "ime1".
  SetCurrentIme("ime1", {info1, info2});
  EXPECT_EQ(UTF8ToUTF16("US"), GetTrayText());

  // Changes the input method to a third-party IME extension.
  SetCurrentIme("ime2", {info1, info2});
  EXPECT_EQ(UTF8ToUTF16("UK*"), GetTrayText());
}

// Tests that IME menu tray changes background color when tapped/clicked. And
// tests that the background color becomes 'inactive' when disabling the IME
// menu feature. Also makes sure that the shelf won't autohide as long as the
// IME menu is open.
TEST_F(ImeMenuTrayTest, PerformAction) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_FALSE(status->ShouldShowShelf());

  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  // Auto-hidden shelf would be forced to be visible as long as the bubble is
  // open.
  EXPECT_TRUE(status->ShouldShowShelf());

  GetTray()->PerformAction(tap);
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(IsBubbleShown());

  // If disabling the IME menu feature when the menu tray is activated, the tray
  // element will be deactivated.
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(IsTrayBackgroundActive());
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(false);
  EXPECT_FALSE(IsVisible());
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(status->ShouldShowShelf());
}

// Tests that IME menu list updates when changing the current IME. This should
// only happen by using shortcuts (Ctrl + Space / Ctrl + Shift + Space) to
// switch IMEs.
TEST_F(ImeMenuTrayTest, RefreshImeWithListViewCreated) {
  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);

  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  mojom::ImeInfo info1, info2, info3;
  info1.id = "ime1";
  info1.name = UTF8ToUTF16("English");
  info1.medium_name = UTF8ToUTF16("English");
  info1.short_name = UTF8ToUTF16("US");
  info1.third_party = false;

  info2.id = "ime2";
  info2.name = UTF8ToUTF16("English UK");
  info2.medium_name = UTF8ToUTF16("English UK");
  info2.short_name = UTF8ToUTF16("UK");
  info2.third_party = true;

  info3.id = "ime3";
  info3.name = UTF8ToUTF16("Pinyin");
  info3.medium_name = UTF8ToUTF16("Chinese Pinyin");
  info3.short_name = UTF8ToUTF16("拼");
  info3.third_party = false;

  std::vector<mojom::ImeInfo> ime_info_list{info1, info2, info3};

  // Switch to ime1.
  SetCurrentIme("ime1", ime_info_list);
  EXPECT_EQ(UTF8ToUTF16("US"), GetTrayText());
  ExpectValidImeList(ime_info_list, info1);

  // Switch to ime3.
  SetCurrentIme("ime3", ime_info_list);
  EXPECT_EQ(UTF8ToUTF16("拼"), GetTrayText());
  ExpectValidImeList(ime_info_list, info3);

  // Closes the menu before quitting.
  GetTray()->PerformAction(tap);
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(IsBubbleShown());
}

// Tests that quits Chrome with IME menu openned will not crash.
TEST_F(ImeMenuTrayTest, QuitChromeWithMenuOpen) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());
}

// Tests using 'Alt+Shift+K' to open the menu.
TEST_F(ImeMenuTrayTest, TestAccelerator) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      SHOW_IME_MENU_BUBBLE, {});
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(IsBubbleShown());
}

TEST_F(ImeMenuTrayTest, ShowingEmojiKeysetHidesBubble) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  TestImeControllerClient client;
  Shell::Get()->ime_controller()->SetClient(client.CreateRemote());
  GetTray()->ShowKeyboardWithKeyset(
      chromeos::input_method::mojom::ImeKeyset::kEmoji);

  // The menu should be hidden.
  EXPECT_FALSE(IsBubbleShown());
}

// Tests that tapping the emoji button does not crash. http://crbug.com/739630
TEST_F(ImeMenuTrayTest, TapEmojiButton) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  Shell::Get()->ime_controller()->SetExtraInputOptionsEnabledState(
      true /* ui enabled */, true /* emoji input enabled */,
      true /* hanwriting input enabled */, true /* voice input enabled */);

  // Open the menu.
  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);

  // Tap the emoji button.
  views::Button* emoji_button = GetEmojiButton();
  ASSERT_TRUE(emoji_button);
  emoji_button->OnGestureEvent(&tap);

  // The menu should be hidden.
  EXPECT_FALSE(IsBubbleShown());
}

TEST_F(ImeMenuTrayTest, ShouldShowBottomButtons) {
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

TEST_F(ImeMenuTrayTest, ShouldShowBottomButtonsSeperate) {
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

}  // namespace ash
