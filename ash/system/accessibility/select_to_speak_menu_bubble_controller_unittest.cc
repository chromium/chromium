// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak_menu_bubble_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/accessibility/select_to_speak_menu_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/accessibility_features.h"
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
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kSelectToSpeakNavigationControl);
    Shell::Get()->accessibility_controller()->select_to_speak().SetEnabled(
        true);
  }

  AccessibilityControllerImpl* GetAccessibilitController() {
    return Shell::Get()->accessibility_controller();
  }

  SelectToSpeakMenuBubbleController* GetBubbleController() {
    return GetAccessibilitController()
        ->GetSelectToSpeakMenuBubbleControllerForTest();
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

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SelectToSpeakMenuBubbleControllerTest, ShowSelectToSpeakPanel_paused) {
  gfx::Rect anchor_rect(10, 10, 0, 0);
  GetAccessibilitController()->ShowSelectToSpeakPanel(anchor_rect,
                                                      /* isPaused= */ true);
  EXPECT_TRUE(GetMenuView());

  FloatingMenuButton* pause_button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kPause);
  EXPECT_EQ(pause_button->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_ASH_SELECT_TO_SPEAK_RESUME));
  EXPECT_TRUE(GetBubbleWidget()->IsVisible());
}

TEST_F(SelectToSpeakMenuBubbleControllerTest,
       ShowSelectToSpeakPanel_notPaused) {
  gfx::Rect anchor_rect(10, 10, 0, 0);
  GetAccessibilitController()->ShowSelectToSpeakPanel(anchor_rect,
                                                      /* isPaused= */ false);
  EXPECT_TRUE(GetMenuView());

  FloatingMenuButton* pause_button =
      GetMenuButton(SelectToSpeakMenuView::ButtonId::kPause);
  EXPECT_EQ(pause_button->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_ASH_SELECT_TO_SPEAK_PAUSE));
  EXPECT_TRUE(GetBubbleWidget()->IsVisible());
}

TEST_F(SelectToSpeakMenuBubbleControllerTest, HideSelectToSpeakPanel) {
  gfx::Rect anchor_rect(10, 10, 0, 0);
  GetAccessibilitController()->ShowSelectToSpeakPanel(anchor_rect,
                                                      /* isPaused= */ false);
  GetAccessibilitController()->HideSelectToSpeakPanel();
  EXPECT_TRUE(GetMenuView());
  EXPECT_FALSE(GetBubbleWidget()->IsVisible());
}

}  // namespace ash