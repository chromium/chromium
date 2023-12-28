// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak/select_to_speak_speed_bubble_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_menu_bubble_controller.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_menu_view.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_speed_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class SelectToSpeakSpeedBubbleControllerTest : public AshTestBase {
 public:
  SelectToSpeakSpeedBubbleControllerTest() = default;
  ~SelectToSpeakSpeedBubbleControllerTest() override = default;

  SelectToSpeakSpeedBubbleControllerTest(
      const SelectToSpeakSpeedBubbleControllerTest&) = delete;
  SelectToSpeakSpeedBubbleControllerTest& operator=(
      const SelectToSpeakSpeedBubbleControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->select_to_speak().SetEnabled(
        true);
  }

  void TearDown() override {
    GetAccessibilitController()->HideSelectToSpeakPanel();
    AshTestBase::TearDown();
  }

  AccessibilityController* GetAccessibilitController() {
    return Shell::Get()->accessibility_controller();
  }

  SelectToSpeakMenuBubbleController* GetMenuBubbleController() {
    return GetAccessibilitController()
        ->GetSelectToSpeakMenuBubbleControllerForTest();
  }

  SelectToSpeakSpeedBubbleController* GetSpeedBubbleController() {
    return GetMenuBubbleController()->speed_bubble_controller_.get();
  }

  views::Widget* GetBubbleWidget() {
    return GetSpeedBubbleController()->bubble_widget_;
  }

  SelectToSpeakMenuView* GetMenuView() {
    return GetMenuBubbleController()->menu_view_;
  }

  FloatingMenuButton* GetMenuButton(SelectToSpeakMenuView::ButtonId view_id) {
    SelectToSpeakMenuView* menu_view = GetMenuView();
    if (!menu_view)
      return nullptr;
    return static_cast<FloatingMenuButton*>(
        menu_view->GetViewByID(static_cast<int>(view_id)));
  }

  void ShowSelectToSpeakSpeedBubble(double rate) {
    gfx::Rect anchor_rect(10, 10, 0, 0);
    GetAccessibilitController()->ShowSelectToSpeakPanel(
        anchor_rect, /*is_paused=*/false, /*speech_rate=*/rate);
    FloatingMenuButton* speed_button =
        GetMenuButton(SelectToSpeakMenuView::ButtonId::kSpeed);
    GetEventGenerator()->GestureTapAt(
        speed_button->GetBoundsInScreen().CenterPoint());
  }

  SelectToSpeakSpeedView* GetSpeedView() {
    return GetSpeedBubbleController()->speed_view_;
  }

  HoverHighlightView* GetOption(int view_id) {
    SelectToSpeakSpeedView* speed_view = GetSpeedView();
    if (!speed_view)
      return nullptr;
    return static_cast<HoverHighlightView*>(speed_view->GetViewByID(view_id));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SelectToSpeakSpeedBubbleControllerTest, SelectSlowOption) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakSpeedBubble(/*rate=*/1.2);

  GetEventGenerator()->GestureTapAt(
      GetOption(1)->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kChangeSpeed);
  EXPECT_EQ(client.last_select_to_speak_panel_action_value(), 0.5);
}

TEST_F(SelectToSpeakSpeedBubbleControllerTest, SelectNormalOption) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakSpeedBubble(/*rate=*/1.2);

  GetEventGenerator()->GestureTapAt(
      GetOption(2)->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kChangeSpeed);
  EXPECT_EQ(client.last_select_to_speak_panel_action_value(), 1.0);
}

TEST_F(SelectToSpeakSpeedBubbleControllerTest, SelectPeppyOption) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakSpeedBubble(/*rate=*/1.2);

  GetEventGenerator()->GestureTapAt(
      GetOption(3)->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kChangeSpeed);
  EXPECT_EQ(client.last_select_to_speak_panel_action_value(), 1.2);
}

TEST_F(SelectToSpeakSpeedBubbleControllerTest, SelectFastOption) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakSpeedBubble(/*rate=*/1.2);

  GetEventGenerator()->GestureTapAt(
      GetOption(4)->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kChangeSpeed);
  EXPECT_EQ(client.last_select_to_speak_panel_action_value(), 1.5);
}

TEST_F(SelectToSpeakSpeedBubbleControllerTest, SelectFasterOption) {
  TestAccessibilityControllerClient client;
  ShowSelectToSpeakSpeedBubble(/*rate=*/1.2);

  GetEventGenerator()->GestureTapAt(
      GetOption(5)->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(client.last_select_to_speak_panel_action(),
            SelectToSpeakPanelAction::kChangeSpeed);
  EXPECT_EQ(client.last_select_to_speak_panel_action_value(), 2.0);
}

TEST_F(SelectToSpeakSpeedBubbleControllerTest, FocusRestoredToSpeedButton) {
  ShowSelectToSpeakSpeedBubble(/*rate=*/1.2);

  GetEventGenerator()->GestureTapAt(
      GetOption(5)->GetBoundsInScreen().CenterPoint());

  FloatingMenuButton* speed_button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kSpeed);
  EXPECT_TRUE(speed_button->HasFocus());
}

}  // namespace ash
