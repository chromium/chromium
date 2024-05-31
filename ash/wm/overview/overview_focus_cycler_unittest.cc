// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_focus_cycler.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/desks/desk_action_button.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/legacy_desk_bar_view.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class OverviewFocusCyclerTest : public OverviewTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  OverviewFocusCyclerTest() = default;
  OverviewFocusCyclerTest(const OverviewFocusCyclerTest&) = delete;
  OverviewFocusCyclerTest& operator=(const OverviewFocusCyclerTest&) = delete;
  ~OverviewFocusCyclerTest() override = default;

  // Helper to make tests more readable.
  bool AreDeskTemplatesEnabled() const { return GetParam(); }

  // OverviewTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kDesksTemplates, AreDeskTemplatesEnabled()},
         {features::kOverviewNewFocus, true},
         {features::kSnapGroup, true}});
    OverviewTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Temporary test to make sure we don't have critical problems with the flag
// enabled. These should be covered in separate tests once the feature is done.
// TODO(http://b/325335020): Remove this test once the feature is complete.
TEST_P(OverviewFocusCyclerTest, NoCrashOnTab) {
  auto* desk_controller = DesksController::Get();
  desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desk_controller->desks().size());

  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window2 = CreateAppWindow();
  ToggleOverview();
  for (int i = 0; i < 15; ++i) {
    PressAndReleaseKey(ui::VKEY_TAB);
  }
}

// ----------------------------------------------------------------------------
// DesksOverviewFocusCyclerTest:

class DesksOverviewFocusCyclerTest : public OverviewFocusCyclerTest {
 public:
  DesksOverviewFocusCyclerTest() = default;
  DesksOverviewFocusCyclerTest(const DesksOverviewFocusCyclerTest&) = delete;
  DesksOverviewFocusCyclerTest& operator=(const DesksOverviewFocusCyclerTest&) =
      delete;
  ~DesksOverviewFocusCyclerTest() override = default;

  views::View* GetHighlightedView() {
    return GetOverviewSession()->focus_cycler()->GetOverviewFocusedView();
  }

  const LegacyDeskBarView* GetDesksBarViewForRoot(aura::Window* root_window) {
    OverviewGrid* grid =
        GetOverviewSession()->GetGridWithRootWindow(root_window);
    const LegacyDeskBarView* bar_view = grid->desks_bar_view();
    CHECK(bar_view->IsZeroState() ^ grid->IsDesksBarViewActive());
    return bar_view;
  }

  // OverviewFocusCyclerOldTest:
  void SetUp() override {
    OverviewFocusCyclerTest::SetUp();

    // All tests in this suite require the desks bar to be visible in overview,
    // which requires at least two desks.
    auto* desk_controller = DesksController::Get();
    desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
    ASSERT_EQ(2u, desk_controller->desks().size());

    // Give the second desk a name. The desk name gets exposed as the accessible
    // name. And the focusable views that are painted in these tests will fail
    // the accessibility paint checker checks if they lack an accessible name.
    desk_controller->GetDeskAtIndex(1)->SetName(u"Desk 2", false);
  }

 protected:
  static void CheckDeskBarViewSize(const LegacyDeskBarView* view,
                                   const std::string& scope) {
    SCOPED_TRACE(scope);
    EXPECT_EQ(view->bounds().height(),
              view->GetWidget()->GetWindowBoundsInScreen().height());
  }
};

// Tests that we can tab through the desk mini views, new desk button and other
// desk items in the correct order.
TEST_P(DesksOverviewFocusCyclerTest, TabbingBasic) {
  OverviewController::Get()->set_disable_app_id_check_for_saved_desks_for_test(
      true);

  std::unique_ptr<aura::Window> window1(CreateAppWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(CreateAppWindow(gfx::Rect(200, 200)));

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());

  CheckDeskBarViewSize(desk_bar_view, "initial");
  ASSERT_EQ(2u, desk_bar_view->mini_views().size());

  // Tests that the first focused desk item is the first desk preview view.
  DeskMiniView* first_mini_view = desk_bar_view->mini_views()[0];
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(first_mini_view->desk_preview(), GetHighlightedView());
  CheckDeskBarViewSize(desk_bar_view, "first mini view");

  // Tests that the combine desks and close all buttons of the first desk
  // preview is focused next.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(first_mini_view->desk_action_view()->combine_desks_button(),
            GetHighlightedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(first_mini_view->desk_action_view()->close_all_button(),
            GetHighlightedView());

  // Test that one more tab focuses the desks name view.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(first_mini_view->desk_name_view(), GetHighlightedView());

  // Tab three times through the second mini view (it has no windows so there is
  // no combine desks button). The next tab should focus the new desk button.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->new_desk_button(), GetHighlightedView());
  CheckDeskBarViewSize(desk_bar_view, "new desk button");

  // Tests that tabbing past the new desk button, we focus the save to a new
  // desk template. The templates button is not in the tab traversal since it is
  // hidden when we have no templates.
  if (AreDeskTemplatesEnabled()) {
    PressAndReleaseKey(ui::VKEY_TAB);
    EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskAsTemplateButton(),
              GetHighlightedView());
  }

  // Tests that after the save desk as template button (if the feature was
  // enabled), focus goes to the save desk for later button.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskForLaterButton(),
            GetHighlightedView());

  OverviewController::Get()->set_disable_app_id_check_for_saved_desks_for_test(
      false);
}

// Tests that we can reverse tab through the desk mini views, new desk button
// and overview items in the correct order.
TEST_P(DesksOverviewFocusCyclerTest, TabbingReverse) {
  OverviewController::Get()->set_disable_app_id_check_for_saved_desks_for_test(
      true);

  std::unique_ptr<aura::Window> window1(CreateAppWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(CreateAppWindow(gfx::Rect(200, 200)));

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_EQ(2u, desk_bar_view->mini_views().size());

  // Tests that the first focused item when reversing is the save desk for
  // later button.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskForLaterButton(),
            GetHighlightedView());

  // Tests that after the save desk for later button, we get the save desk as
  // template button, if the feature is enabled.
  if (AreDeskTemplatesEnabled()) {
    PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
    EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskAsTemplateButton(),
              GetHighlightedView());
  }

  // Tests that after the desks templates button (if the feature was enabled),
  // we get to the new desk button.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->new_desk_button(), GetHighlightedView());

  // Tests that after the new desk button comes the preview views, desk action
  // buttons, and the desk name views in reverse order.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  DeskMiniView* second_mini_view = desk_bar_view->mini_views()[1];
  EXPECT_EQ(second_mini_view->desk_name_view(), GetHighlightedView());
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(second_mini_view->desk_action_view()->close_all_button(),
            GetHighlightedView());
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(second_mini_view->desk_preview(), GetHighlightedView());

  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  DeskMiniView* first_mini_view = desk_bar_view->mini_views()[0];
  EXPECT_EQ(first_mini_view->desk_name_view(), GetHighlightedView());
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(first_mini_view->desk_action_view()->close_all_button(),
            GetHighlightedView());
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(first_mini_view->desk_action_view()->combine_desks_button(),
            GetHighlightedView());
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(first_mini_view->desk_preview(), GetHighlightedView());

  // TODO(http://b/325335020): Tabbing onto overview items is not supported
  // yet.

  OverviewController::Get()->set_disable_app_id_check_for_saved_desks_for_test(
      false);
}

TEST_P(DesksOverviewFocusCyclerTest, MiniViewAccelerator) {
  // We are initially on desk 1.
  const auto* desks_controller = DesksController::Get();
  auto& desks = desks_controller->desks();
  ASSERT_EQ(desks_controller->active_desk(), desks[0].get());

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());

  // Use keyboard to navigate to the preview view associated with desk 2.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(desk_bar_view->mini_views()[1]->desk_preview(),
            GetHighlightedView());

  // Tests that after hitting the space key on the focused preview view
  // associated with desk 2, we switch to desk 2.
  PressAndReleaseKey(ui::VKEY_SPACE);
  DeskSwitchAnimationWaiter().Wait();
  EXPECT_EQ(desks_controller->active_desk(), desks[1].get());
}

TEST_P(DesksOverviewFocusCyclerTest, CloseDeskWithMiniViewAccelerator) {
  const auto* desks_controller = DesksController::Get();
  ASSERT_EQ(2u, desks_controller->desks().size());
  auto* desk1 = desks_controller->GetDeskAtIndex(0);
  auto* desk2 = desks_controller->GetDeskAtIndex(1);
  ASSERT_EQ(desk1, desks_controller->active_desk());

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  auto* mini_view2 = desk_bar_view->mini_views()[1].get();

  // Use keyboard to navigate to the miniview associated with desk 2.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(mini_view2->desk_preview(), GetHighlightedView());

  // Tests that after hitting ctrl-w on the focused preview view associated with
  // `desk2`, `desk2` is destroyed.
  PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(1u, desks_controller->desks().size());
  EXPECT_NE(desk2, desks_controller->GetDeskAtIndex(0));

  // Desks bar never goes back to zero state after it's initialized.
  EXPECT_FALSE(desk_bar_view->IsZeroState());
  EXPECT_FALSE(desk_bar_view->mini_views().empty());
}

TEST_P(DesksOverviewFocusCyclerTest, DeskNameView) {
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  auto* desk_name_view_1 = desk_bar_view->mini_views()[0]->desk_name_view();

  auto* desk_1 = DesksController::Get()->GetDeskAtIndex(0);
  const std::u16string original_name = desk_1->name();

  // Tab until the desk name view of the first desk is focused. Verify that the
  // desk name is being edited.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_name_view_1, GetHighlightedView());
  EXPECT_TRUE(desk_name_view_1->HasFocus());
  EXPECT_TRUE(desk_bar_view->IsDeskNameBeingModified());

  // The whole name starts off selected.
  EXPECT_TRUE(desk_name_view_1->HasSelection());
  EXPECT_EQ(original_name, desk_name_view_1->GetSelectedText());

  // Left and right arrow keys should not change neither the focus as they need
  // to move the text caret.
  PressAndReleaseKey(ui::VKEY_RIGHT);
  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_EQ(desk_name_view_1, GetHighlightedView());
  EXPECT_TRUE(desk_name_view_1->HasFocus());

  // Tests ctrl + A and backspace to select all and delete.
  PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_BACK);
  EXPECT_EQ(u"", desk_name_view_1->GetText());

  // Type "code" into the desk name textfield. It should update, but the desk
  // name has not change.
  PressAndReleaseKey(ui::VKEY_C);
  PressAndReleaseKey(ui::VKEY_O);
  PressAndReleaseKey(ui::VKEY_D);
  PressAndReleaseKey(ui::VKEY_E);
  EXPECT_EQ(u"code", desk_name_view_1->GetText());
  EXPECT_EQ(original_name, desk_1->name());

  // Tests that pressing tab will advance focus and commit the desk name
  // changes.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_FALSE(desk_name_view_1->HasFocus());
  EXPECT_EQ(u"code", desk_1->name());
  EXPECT_TRUE(desk_1->is_name_set_by_user());
}

TEST_P(DesksOverviewFocusCyclerTest, RemoveDeskWhileNameFocused) {
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  auto* desk_name_view_1 = desk_bar_view->mini_views()[0]->desk_name_view();

  // Tab until the desk name view of the first desk is focused.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_name_view_1, GetHighlightedView());

  // Desks bar never goes back to zero state after it's initialized.
  const auto* desks_controller = DesksController::Get();
  auto* desk_1 = desks_controller->GetDeskAtIndex(0);
  RemoveDesk(desk_1);
  EXPECT_EQ(nullptr, GetHighlightedView());
  EXPECT_FALSE(desk_bar_view->IsZeroState());

  // Tabbing again should cause no crashes.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->mini_views()[0]->desk_preview(),
            GetHighlightedView());
}

// Tests the overview focus cycler behavior when a user uses the new desk
// button.
TEST_P(DesksOverviewFocusCyclerTest, NewDesksWithKeyboard) {
  // Make sure the display is large enough to hold the max number of desks.
  UpdateDisplay("1200x800");

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(desk_bar_view->IsZeroState());
  const views::LabelButton* new_desk_button = desk_bar_view->new_desk_button();
  const auto* desks_controller = DesksController::Get();

  auto check_name_view_at_index = [this, desks_controller](
                                      const auto* desk_bar_view, int index) {
    const auto* desk_name_view =
        desk_bar_view->mini_views()[index]->desk_name_view();
    EXPECT_TRUE(desk_name_view->HasFocus());
    if (desks_controller->CanCreateDesks()) {
      EXPECT_EQ(desk_name_view, GetHighlightedView());
    }
    EXPECT_EQ(std::u16string(), desk_name_view->GetText());
  };

  // Tab seven times, three times for each desk (preview, close button, name),
  // and then one more time to focus the new desk button.
  for (int i = 0; i < 7; ++i) {
    PressAndReleaseKey(ui::VKEY_TAB);
  }
  ASSERT_EQ(new_desk_button, GetHighlightedView());

  // Keep adding new desks until we reach the maximum allowed amount. Verify the
  // amount of desks is indeed the maximum allowed and that the new desk button
  // is disabled.
  while (desks_controller->CanCreateDesks()) {
    PressAndReleaseKey(ui::VKEY_SPACE);
    check_name_view_at_index(desk_bar_view,
                             desks_controller->desks().size() - 1);
    PressAndReleaseKey(ui::VKEY_TAB);
  }
  EXPECT_FALSE(new_desk_button->GetEnabled());
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks(),
            desks_controller->desks().size());
}

TEST_P(DesksOverviewFocusCyclerTest, ZeroStateOfDesksBar) {
  ToggleOverview();
  auto* desks_bar_view = GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(desks_bar_view->IsZeroState());
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Remove one desk to enter zero state desks bar.
  auto* mini_view = desks_bar_view->mini_views()[1].get();
  GetEventGenerator()->MoveMouseTo(
      mini_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetDeskActionVisibilityForMiniView(mini_view));
  LeftClickOn(GetCloseDeskButtonForMiniView(mini_view));

  // Desks bar never goes back to zero state after it's initialized.
  ASSERT_FALSE(desks_bar_view->IsZeroState());

  // Exit and reenter overview to show the zero state desks bar.
  ToggleOverview();
  ToggleOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Tab through and verify the zero state desk bar views.
  desks_bar_view = GetOverviewSession()
                       ->GetGridWithRootWindow(Shell::GetPrimaryRootWindow())
                       ->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desks_bar_view->default_desk_button(), GetHighlightedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desks_bar_view->new_desk_button(), GetHighlightedView());

  // Test that when we create a new desk, we focus the desk name view of that
  // desk.
  PressAndReleaseKey(ui::VKEY_SPACE);
  EXPECT_EQ(desks_bar_view->mini_views()[1]->desk_name_view(),
            GetHighlightedView());
}

TEST_P(DesksOverviewFocusCyclerTest, ClickingNameViewMovesFocus) {
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  CheckDeskBarViewSize(desk_bar_view, "initial");
  ASSERT_EQ(2u, desk_bar_view->mini_views().size());

  // Tab to first mini desk view's preview view.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(desk_bar_view->mini_views()[0]->desk_preview(),
            GetHighlightedView());
  CheckDeskBarViewSize(desk_bar_view, "tabbed once");

  // Click on the second mini desk item's name view.
  auto* event_generator = GetEventGenerator();
  auto* desk_name_view_1 = desk_bar_view->mini_views()[1]->desk_name_view();
  event_generator->MoveMouseTo(
      desk_name_view_1->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_FALSE(desk_bar_view->IsZeroState());

  // Verify that focus has moved to the clicked desk item.
  EXPECT_EQ(desk_name_view_1, GetHighlightedView());
}

// Tests that there is no crash when tabbing after we switch to the zero state
// desks bar. Regression test for https://crbug.com/1301134.
TEST_P(DesksOverviewFocusCyclerTest, SwitchingToZeroStateWhileTabbing) {
  ToggleOverview();
  auto* desks_bar_view = GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(desks_bar_view->IsZeroState());
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Tab to first mini desk view's preview view.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(desks_bar_view->mini_views()[0]->desk_preview(),
            GetHighlightedView());

  // Remove one desk to have only one desk left.
  auto* mini_view = desks_bar_view->mini_views()[1].get();
  GetEventGenerator()->MoveMouseTo(
      mini_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(GetDeskActionVisibilityForMiniView(mini_view));
  LeftClickOn(GetCloseDeskButtonForMiniView(mini_view));

  // Desks bar never goes back to zero state after it's initialized.
  ASSERT_FALSE(desks_bar_view->IsZeroState());

  // Try tabbing after removing the second desk triggers us to transition to
  // zero state desks bar. There should not be a crash.
  PressAndReleaseKey(ui::VKEY_TAB);
}

INSTANTIATE_TEST_SUITE_P(All, OverviewFocusCyclerTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, DesksOverviewFocusCyclerTest, testing::Bool());

}  // namespace ash
