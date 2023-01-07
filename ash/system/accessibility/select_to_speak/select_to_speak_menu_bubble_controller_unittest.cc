// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak/select_to_speak_menu_bubble_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_menu_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

class SelectToSpeakMenuBubbleControllerTest : public AshTestBase {
 public:
  SelectToSpeakMenuBubbleControllerTest() = default;
  ~SelectToSpeakMenuBubbleControllerTest() override = default;

  SelectToSpeakMenuBubbleControllerTest(
      const SelectToSpeakMenuBubbleControllerTest&) = delete;
  SelectToSpeakMenuBubbleControllerTest& operator=(
      const SelectToSpeakMenuBubbleControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->select_to_speak().SetEnabled(
        true);
  }

  void TearDown() override { AshTestBase::TearDown(); }

  AccessibilityControllerImpl* GetAccessibilitController() {
    return Shell::Get()->accessibility_controller();
  }

  SelectToSpeakMenuBubbleController* GetBubbleController() {
    return GetAccessibilitController()
        ->GetSelectToSpeakMenuBubbleControllerForTest();
  }

  SelectToSpeakSpeedBubbleController* GetSpeedBubbleController() {
    return GetBubbleController()->speed_bubble_controller_.get();
  }

  views::Widget* GetBubbleWidget() {
    return GetBubbleController()->bubble_widget_;
  }

  SelectToSpeakMenuView* GetMenuView() {
    return GetBubbleController()->menu_view_;
  }

  FloatingMenuButton* GetMenuButton(SelectToSpeakMenuView::ButtonId view_id) {
    SelectToSpeakMenuView* menu_view = GetMenuView();
    if (!menu_view)
      return nullptr;
    return static_cast<FloatingMenuButton*>(
        menu_view->GetViewByID(static_cast<int>(view_id)));
  }

  void ShowSelectToSpeakPanel(bool is_paused) {
    gfx::Rect anchor_rect(10, 10, 0, 0);
    GetAccessibilitController()->ShowSelectToSpeakPanel(anchor_rect, is_paused,
                                                        /*speech_rate=*/1.2);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SelectToSpeakMenuBubbleControllerTest, ShowSelectToSpeakPanel_paused) {
  ShowSelectToSpeakPanel(/*is_paused=*/true);
  EXPECT_TRUE(GetMenuView());

  FloatingMenuButton* pause_button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kPause);
  EXPECT_EQ(pause_button->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_ASH_SELECT_TO_SPEAK_RESUME));
  EXPECT_TRUE(GetBubbleWidget()->IsVisible());
}

TEST_F(SelectToSpeakMenuBubbleControllerTest,
       ShowSelectToSpeakPanel_notPaused) {
  ShowSelectToSpeakPanel(/*is_paused=*/false);
  EXPECT_TRUE(GetMenuView());

  FloatingMenuButton* pause_button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kPause);
  EXPECT_EQ(pause_button->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_ASH_SELECT_TO_SPEAK_PAUSE));
  EXPECT_TRUE(GetBubbleWidget()->IsVisible());
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, HideSelectToSpeakPanel) {
  ShowSelectToSpeakPanel(/*is_paused=*/false);
  GetAccessibilitController()->HideSelectToSpeakPanel();
  EXPECT_TRUE(GetMenuView());
  EXPECT_FALSE(GetBubbleWidget()->IsVisible());
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, PauseButtonPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);
  FloatingMenuButton* button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kPause);
  GetEventGenerator()->GestureTapAt(button->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kPause);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, ResumeButtonPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/true);
  FloatingMenuButton* button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kPause);
  GetEventGenerator()->GestureTapAt(button->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kResume);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, PrevParagraphButtonPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);
  FloatingMenuButton* button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kPrevParagraph);
  GetEventGenerator()->GestureTapAt(button->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kPreviousParagraph);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, PrevParagraphKeyPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);

  GetEventGenerator()->PressKey(ui::VKEY_UP, ui::EF_NONE);

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kPreviousParagraph);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, PrevSentenceButtonPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);
  FloatingMenuButton* button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kPrevSentence);
  GetEventGenerator()->GestureTapAt(button->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kPreviousSentence);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, PrevSentenceKeyPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);

  GetEventGenerator()->PressKey(ui::VKEY_LEFT, ui::EF_NONE);

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kPreviousSentence);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, PrevSentenceKeyPressedRtl) {
  base::i18n::SetICUDefaultLocale("he");
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);

  GetEventGenerator()->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kPreviousSentence);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, NextParagraphButtonPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);
  FloatingMenuButton* button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kNextParagraph);
  GetEventGenerator()->GestureTapAt(button->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kNextParagraph);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, NextParagraphKeyPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);

  GetEventGenerator()->PressKey(ui::VKEY_DOWN, ui::EF_NONE);

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kNextParagraph);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, NextSentenceButtonPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);
  FloatingMenuButton* button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kNextSentence);
  GetEventGenerator()->GestureTapAt(button->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kNextSentence);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, NextSentenceKeyPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);

  GetEventGenerator()->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kNextSentence);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, NextSentenceKeyPressedRtl) {
  base::i18n::SetICUDefaultLocale("he");
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);

  GetEventGenerator()->PressKey(ui::VKEY_LEFT, ui::EF_NONE);

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kNextSentence);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, StopButtonPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);
  FloatingMenuButton* button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kStop);
  GetEventGenerator()->GestureTapAt(button->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kExit);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, StopKeyPressed) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakPanel(/*is_paused=*/false);

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kExit);
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, ChangeSpeedButtonPressed) {
  ShowSelectToSpeakPanel(/*is_paused=*/false);
  FloatingMenuButton* button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kSpeed);
  GetEventGenerator()->GestureTapAt(button->GetBoundsInScreen().CenterPoint());

  EXPECT_TRUE(GetSpeedBubbleController() &&
              GetSpeedBubbleController()->IsVisible());

  // Clicking button again closes the speed selection bubble.
  GetEventGenerator()->GestureTapAt(button->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(!GetSpeedBubbleController() ||
              !GetSpeedBubbleController()->IsVisible());
}

}  // namespace ash
