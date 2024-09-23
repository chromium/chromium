// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/scrollable_shelf_constants.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shelf/test/shelf_test_base.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event_utils.h"
#include "ui/views/animation/ink_drop.h"

namespace ash {
namespace {

// A helper class to override the current time.
struct TimeOverrideHelper {
  static base::Time TimeNow() { return current_time; }

  // Used as the current time in ash pixel diff tests.
  static base::Time current_time;
};

base::Time TimeOverrideHelper::current_time;
}  // namespace

class PageFlipWaiter : public ScrollableShelfView::TestObserver {
 public:
  explicit PageFlipWaiter(ScrollableShelfView* scrollable_shelf_view)
      : scrollable_shelf_view_(scrollable_shelf_view) {
    scrollable_shelf_view->SetTestObserver(this);
  }
  ~PageFlipWaiter() override {
    scrollable_shelf_view_->SetTestObserver(nullptr);
  }

  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  void OnPageFlipTimerFired() override {
    DCHECK(run_loop_.get());
    run_loop_->Quit();
  }

  raw_ptr<ScrollableShelfView> scrollable_shelf_view_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class InkDropAnimationWaiter : public views::InkDropObserver {
 public:
  explicit InkDropAnimationWaiter(views::Button* button) : button_(button) {
    views::InkDrop::Get(button)->GetInkDrop()->AddObserver(this);
  }
  ~InkDropAnimationWaiter() override {
    views::InkDrop::Get(button_)->GetInkDrop()->RemoveObserver(this);
  }

  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  void InkDropAnimationStarted() override {}
  void InkDropRippleAnimationEnded(
      views::InkDropState ink_drop_state) override {
    if (ink_drop_state != views::InkDropState::ACTIVATED &&
        ink_drop_state != views::InkDropState::HIDDEN) {
      return;
    }
    if (run_loop_.get()) {
      run_loop_->Quit();
    }
  }

  raw_ptr<views::Button> button_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(const ShelfID& shelf_id)
      : ShelfItemDelegate(shelf_id) {}

  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override {
    std::move(callback).Run(SHELF_ACTION_WINDOW_ACTIVATED, {});
  }
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {}
  void Close() override {}
};

class ScrollableShelfViewTest : public ShelfTestBase {
 public:
  ScrollableShelfViewTest() = default;
  ~ScrollableShelfViewTest() override = default;

 protected:
  ShelfID AddAppShortcut(ShelfItemType item_type = TYPE_PINNED_APP) {
    return AddAppShortcutWithIconColor(item_type, SK_ColorRED);
  }

  // Verifies that a tappable app icon should have the correct button state when
  // it is under mouse hover.
  void VerifyTappableAppIconsUnderMouseHover() {
    // Ensure that the mouse does not hover any app icon.
    GetEventGenerator()->MoveMouseTo(
        GetPrimaryDisplay().bounds().CenterPoint());

    for (size_t i = scrollable_shelf_view_->first_tappable_app_index().value();
         i <= scrollable_shelf_view_->last_tappable_app_index().value(); ++i) {
      const auto* shelf_app_icon = test_api_->GetButton(i);
      const gfx::Rect app_icon_screen_bounds =
          shelf_app_icon->GetBoundsInScreen();
      const gfx::Point app_icon_screen_center(
          app_icon_screen_bounds.CenterPoint());

      // `shelf_app_icon` is not hovered.
      ASSERT_EQ(views::Button::STATE_NORMAL, shelf_app_icon->GetState());

      // Move the mouse to `app_icon_screen_center` and verify that
      // `shelf_app_icon` is under mouse hover.
      GetEventGenerator()->MoveMouseTo(app_icon_screen_center);
      EXPECT_EQ(views::Button::STATE_HOVERED, shelf_app_icon->GetState());
    }
  }

  void AddAppShortcutsUntilRightArrowIsShown() {
    ASSERT_FALSE(scrollable_shelf_view_->right_arrow()->GetVisible());

    while (!scrollable_shelf_view_->right_arrow()->GetVisible()) {
      AddAppShortcut();
    }
  }

  void CheckFirstAndLastTappableIconsBounds() {
    views::ViewModel* view_model = shelf_view_->view_model_for_test();

    gfx::Rect visible_space_in_screen = scrollable_shelf_view_->visible_space();
    views::View::ConvertRectToScreen(scrollable_shelf_view_,
                                     &visible_space_in_screen);

    views::View* last_tappable_icon = view_model->view_at(
        scrollable_shelf_view_->last_tappable_app_index().value());
    const gfx::Rect last_tappable_icon_bounds =
        last_tappable_icon->GetBoundsInScreen();

    // Expects that the last tappable icon is fully shown.
    EXPECT_TRUE(visible_space_in_screen.Contains(last_tappable_icon_bounds));

    views::View* first_tappable_icon = view_model->view_at(
        scrollable_shelf_view_->first_tappable_app_index().value());
    const gfx::Rect first_tappable_icon_bounds =
        first_tappable_icon->GetBoundsInScreen();

    // Expects that the first tappable icon is fully shown.
    EXPECT_TRUE(visible_space_in_screen.Contains(first_tappable_icon_bounds));
  }

  void VeirifyRippleRingWithinShelfContainer(
      const ShelfAppButton& button) const {
    const gfx::Rect shelf_container_bounds_in_screen =
        scrollable_shelf_view_->shelf_container_view()->GetBoundsInScreen();

    const gfx::Rect small_ripple_area = button.CalculateSmallRippleArea();
    const gfx::Point ripple_center = small_ripple_area.CenterPoint();
    const int ripple_radius = small_ripple_area.width() / 2;

    // Calculate the ripple's left end and right end in screen.
    const int ripple_center_x_in_screen =
        ripple_center.x() + button.GetBoundsInScreen().x();
    const int ripple_x = ripple_center_x_in_screen - ripple_radius;
    const int ripple_right = ripple_center_x_in_screen + ripple_radius;

    // Verify that both ends are within bounds of shelf container view.
    EXPECT_GE(ripple_x, shelf_container_bounds_in_screen.x());
    EXPECT_LE(ripple_right, shelf_container_bounds_in_screen.right());
  }

  bool HasRoundedCornersOnAppButtonAfterMouseRightClick(
      ShelfAppButton* button) {
    const gfx::Point location_within_button =
        button->GetBoundsInScreen().CenterPoint();
    GetEventGenerator()->MoveMouseTo(location_within_button);
    GetEventGenerator()->ClickRightButton();

    ui::Layer* layer = scrollable_shelf_view_->shelf_container_view()->layer();

    // The gfx::RoundedCornersF object is considered empty when all of the
    // corners are squared (no effective radius).
    const bool has_rounded_corners = !(layer->rounded_corner_radii().IsEmpty());

    // Click outside of |button|. Expects that the rounded corners should always
    // be empty.
    GetEventGenerator()->GestureTapAt(
        button->GetBoundsInScreen().bottom_center());
    EXPECT_TRUE(layer->rounded_corner_radii().IsEmpty());

    return has_rounded_corners;
  }
};

// Tests scrollable shelf's features under both LTR and RTL.
class ScrollableShelfViewRTLTest : public ScrollableShelfViewTest,
                                   public testing::WithParamInterface<bool> {
 public:
  ScrollableShelfViewRTLTest() : scoped_locale_(GetParam() ? "ar" : "") {}
  ~ScrollableShelfViewRTLTest() override = default;

 private:
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
};

INSTANTIATE_TEST_SUITE_P(RTL, ScrollableShelfViewRTLTest, testing::Bool());

// Verifies that the display rotation from the short side to the long side
// should not break the scrollable shelf's UI
// behavior(https://crbug.com/1000764).
TEST_F(ScrollableShelfViewTest, CorrectUIAfterDisplayRotationShortToLong) {
  // Changes the display setting in order that the display's height is greater
  // than the width.
  UpdateDisplay("600x800");

  display::Display display = GetPrimaryDisplay();

  // Adds enough app icons so that after display rotation the scrollable
  // shelf is still in overflow mode.
  const int num = display.bounds().height() / shelf_view_->GetButtonSize();
  for (int i = 0; i < num; i++) {
    AddAppShortcut();
  }

  // Because the display's height is greater than the display's width,
  // the scrollable shelf is in overflow mode before display rotation.
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Presses the right arrow until reaching the last page of shelf icons.
  const views::View* right_arrow = scrollable_shelf_view_->right_arrow();
  const gfx::Point center_point =
      right_arrow->GetBoundsInScreen().CenterPoint();
  while (right_arrow->GetVisible()) {
    GetEventGenerator()->MoveMouseTo(center_point);
    GetEventGenerator()->PressLeftButton();
    GetEventGenerator()->ReleaseLeftButton();
  }
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Rotates the display by 90 degrees.
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->SetDisplayRotation(display.id(), display::Display::ROTATE_90,
                                      display::Display::RotationSource::ACTIVE);

  // After rotation, checks the following things:
  // (1) The scrollable shelf has the correct layout strategy.
  // (2) The last app icon has the correct bounds.
  // (3) The scrollable shelf does not need further adjustment.
  EXPECT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  views::ViewModel* view_model = shelf_view_->view_model_for_test();
  const views::View* last_visible_icon = view_model->view_at(
      scrollable_shelf_view_->last_tappable_app_index().value());
  const gfx::Rect icon_bounds = last_visible_icon->GetBoundsInScreen();
  gfx::Rect visible_space = scrollable_shelf_view_->visible_space();
  views::View::ConvertRectToScreen(scrollable_shelf_view_, &visible_space);
  EXPECT_EQ(icon_bounds.right() +
                ShelfConfig::Get()->scrollable_shelf_ripple_padding() +
                ShelfConfig::Get()->GetAppIconEndPadding(),
            visible_space.right());
  EXPECT_FALSE(scrollable_shelf_view_->ShouldAdjustForTest());
}

// Verifies that the gradient calculation for vertical scrollable shelf does not
// crash for minimum shelf height (b/319527955).
TEST_F(ScrollableShelfViewTest,
       GradientCalculationDoesNotCrashForMinimumShelfHeight) {
  const size_t initial_display_height = 400;
  UpdateDisplay(base::StringPrintf("600x%zu", initial_display_height));

  auto* const prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  SetShelfAlignmentPref(prefs, GetPrimaryDisplay().id(), ShelfAlignment::kLeft);
  AddAppShortcutsUntilOverflow();

  // Reduce the screen height (this can also happen with the maximum size of the
  // docked magnifier), so the height of `scrollable_shelf_view_` is only 1 px,
  // this is where the crash happened before.
  const size_t new_display_height =
      initial_display_height -
      scrollable_shelf_view_->visible_space().height() + 1;
  UpdateDisplay(base::StringPrintf("600x%zu", new_display_height));

  // No crash.
}

// TODO(crbug.com/40867071): Enable when the bug is fixed.
// Verifies that the display rotation from the long side to the short side
// should not break the scrollable shelf's UI behavior
// (https://crbug.com/1000764).
TEST_P(ScrollableShelfViewRTLTest,
       DISABLED_CorrectUIAfterDisplayRotationLongToShort) {
  // Changes the display setting in order that the display's width is greater
  // than the height.
  UpdateDisplay("600x300");

  display::Display display = GetPrimaryDisplay();
  AddAppShortcutsUntilOverflow();

  // Presses the right arrow until reaching the last page of shelf icons.
  const views::View* right_arrow = scrollable_shelf_view_->right_arrow();
  const gfx::Point center_point =
      right_arrow->GetBoundsInScreen().CenterPoint();
  while (right_arrow->GetVisible()) {
    GetEventGenerator()->MoveMouseTo(center_point);
    GetEventGenerator()->PressLeftButton();
    GetEventGenerator()->ReleaseLeftButton();
  }
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Rotates the display by 90 degrees. In order to reproduce the bug,
  // both arrow buttons should show after rotation.
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->SetDisplayRotation(display.id(), display::Display::ROTATE_90,
                                      display::Display::RotationSource::ACTIVE);
  ASSERT_EQ(ScrollableShelfView::kShowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Verifies that the scrollable shelf does not need further adjustment.
  EXPECT_FALSE(scrollable_shelf_view_->ShouldAdjustForTest());
}

// Verifies that the mask layer gradient shader is not applied when no arrow
// button shows.
TEST_P(ScrollableShelfViewRTLTest, VerifyApplyMaskGradientShaderWhenNeeded) {
  AddAppShortcut();
  ASSERT_EQ(ScrollableShelfView::LayoutStrategy::kNotShowArrowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());
  EXPECT_TRUE(scrollable_shelf_view_->layer()->gradient_mask().IsEmpty());

  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::LayoutStrategy::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  EXPECT_FALSE(scrollable_shelf_view_->layer()->gradient_mask().IsEmpty());
}

// When hovering mouse on a shelf icon, the tooltip only shows for the visible
// icon (see https://crbug.com/997807).
TEST_P(ScrollableShelfViewRTLTest, NotShowTooltipForHiddenIcons) {
  AddAppShortcutsUntilOverflow();

  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  views::ViewModel* view_model = shelf_view_->view_model_for_test();

  // Check the initial state of |tooltip_manager|.
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Verifies that tooltip should show for a visible shelf item.
  views::View* visible_icon = view_model->view_at(
      scrollable_shelf_view_->first_tappable_app_index().value());
  GetEventGenerator()->MoveMouseTo(
      visible_icon->GetBoundsInScreen().CenterPoint());
  tooltip_manager->ShowTooltip(visible_icon);
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Reset |tooltip_manager|.
  GetEventGenerator()->MoveMouseTo(gfx::Point());
  tooltip_manager->Close();
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Verifies that tooltip should not show for a hidden shelf item.
  views::View* hidden_icon = view_model->view_at(
      scrollable_shelf_view_->last_tappable_app_index().value() + 1);
  GetEventGenerator()->MoveMouseTo(
      hidden_icon->GetBoundsInScreen().CenterPoint());
  tooltip_manager->ShowTooltip(hidden_icon);
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

// Test that tapping near the scroll arrow button triggers scrolling. (see
// https://crbug.com/1004998)
TEST_P(ScrollableShelfViewRTLTest, ScrollAfterTappingNearScrollArrow) {
  AddAppShortcutsUntilOverflow();

  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Tap right arrow and check that the scrollable shelf now shows the left
  // arrow only. Then do the same for the left arrow.
  const gfx::Rect right_arrow =
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen();
  GetEventGenerator()->GestureTapAt(right_arrow.CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  const gfx::Rect left_arrow =
      scrollable_shelf_view_->left_arrow()->GetBoundsInScreen();
  GetEventGenerator()->GestureTapAt(left_arrow.CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Recalculate the right arrow  bounds considering the padding for the tap
  // area.
  const int horizontalPadding = (32 - right_arrow.width()) / 2;
  const int verticalPadding =
      (shelf_view_->GetButtonSize() - right_arrow.height()) / 2;

  // Tap near the right arrow and check that the scrollable shelf now shows the
  // left arrow only. Then do the same for the left arrow.
  GetEventGenerator()->GestureTapAt(
      gfx::Point(right_arrow.top_right().x() - horizontalPadding,
                 right_arrow.top_right().y() + verticalPadding));
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  GetEventGenerator()->GestureTapAt(
      gfx::Point(left_arrow.top_right().x(), left_arrow.top_right().y()));
  EXPECT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
}

// Verifies that in overflow mode, the app icons indexed by
// |first_tappable_app_index_| and |last_tappable_app_index_| are completely
// shown (https://crbug.com/1013811).
TEST_P(ScrollableShelfViewRTLTest, VerifyTappableAppIndices) {
  AddAppShortcutsUntilOverflow();

  // Checks bounds when the layout strategy is kShowRightArrowButton.
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  CheckFirstAndLastTappableIconsBounds();

  // Pins enough apps to Shelf to ensure that layout strategy will be
  // kShowButtons after pressing the right arrow button.
  const int view_size =
      scrollable_shelf_view_->shelf_view()->view_model_for_test()->view_size();
  PopulateAppShortcut(view_size + 1);
  GetEventGenerator()->GestureTapAt(
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen().CenterPoint());

  // Checks bounds when the layout strategy is kShowButtons.
  ASSERT_EQ(ScrollableShelfView::kShowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());
  CheckFirstAndLastTappableIconsBounds();

  GetEventGenerator()->GestureTapAt(
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen().CenterPoint());

  // Checks bounds when the layout strategy is kShowLeftArrowButton.
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  CheckFirstAndLastTappableIconsBounds();
}

TEST_P(ScrollableShelfViewRTLTest, ShowTooltipForArrowButtons) {
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Check the initial state of |tooltip_manager|.
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();
  EXPECT_FALSE(tooltip_manager->IsVisible());

  for (ShelfAlignment alignment :
       {ShelfAlignment::kBottom, ShelfAlignment::kLeft,
        ShelfAlignment::kRight}) {
    SCOPED_TRACE(testing::Message() << "Testing shelf with alignment "
                                    << static_cast<int>(alignment));
    GetPrimaryShelf()->SetAlignment(alignment);

    // Verifies that tooltip should show for a visible shelf item.
    views::View* right_arrow = scrollable_shelf_view_->right_arrow();
    GetEventGenerator()->MoveMouseTo(
        right_arrow->GetBoundsInScreen().CenterPoint());
    tooltip_manager->ShowTooltip(right_arrow);
    EXPECT_TRUE(tooltip_manager->IsVisible());

    // Click right arrow button to scroll the shelf and show left arrow button.
    GetEventGenerator()->ClickLeftButton();
    ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
              scrollable_shelf_view_->layout_strategy_for_test());

    // Reset |tooltip_manager|.
    GetEventGenerator()->MoveMouseTo(gfx::Point());
    tooltip_manager->Close();
    EXPECT_FALSE(tooltip_manager->IsVisible());

    views::View* left_arrow = scrollable_shelf_view_->left_arrow();
    GetEventGenerator()->MoveMouseTo(
        left_arrow->GetBoundsInScreen().CenterPoint());
    tooltip_manager->ShowTooltip(left_arrow);
    EXPECT_TRUE(tooltip_manager->IsVisible());

    tooltip_manager->Close();
    EXPECT_FALSE(tooltip_manager->IsVisible());
  }
}

// Verifies that dragging an app icon to a new shelf page works well. In
// addition, the dragged icon moves with mouse before mouse release (see
// https://crbug.com/1031367).
TEST_P(ScrollableShelfViewRTLTest, DragIconToNewPage) {
  scrollable_shelf_view_->set_page_flip_time_threshold(base::Milliseconds(10));

  AddAppShortcutsUntilOverflow();
  GetEventGenerator()->GestureTapAt(
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen().CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  views::ViewModel* view_model = shelf_view_->view_model_for_test();
  views::View* dragged_view = view_model->view_at(
      scrollable_shelf_view_->last_tappable_app_index().value());
  const gfx::Point drag_start_point =
      dragged_view->GetBoundsInScreen().CenterPoint();

  // Ensures that the app icon is not dragged to the ideal bounds directly.
  // It helps to construct a more complex scenario that the animation
  // is created to move the dropped icon to the target place after drag release.
  const gfx::Point drag_end_point =
      scrollable_shelf_view_->left_arrow()->GetBoundsInScreen().origin() -
      gfx::Vector2d(10, 0);

  ASSERT_NE(0u, view_model->GetIndexOfView(dragged_view));

  // Drag |dragged_view| from |drag_start_point| to |drag_end_point|. Wait
  // for enough time before releasing the mouse button.
  GetEventGenerator()->MoveMouseTo(drag_start_point);
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(drag_end_point);
  {
    PageFlipWaiter waiter(scrollable_shelf_view_);
    waiter.Wait();
  }

  // Expects that the drag icon moves with drag pointer before mouse release.
  const gfx::Rect intermediate_bounds =
      shelf_view_->GetDragIconBoundsInScreenForTest();
  EXPECT_EQ(drag_end_point, intermediate_bounds.CenterPoint());

  GetEventGenerator()->ReleaseLeftButton();
  ASSERT_NE(intermediate_bounds.CenterPoint(),
            dragged_view->GetBoundsInScreen().CenterPoint());

  // Expects that the proxy icon is deleted after mouse release.
  EXPECT_EQ(gfx::Rect(), shelf_view_->GetDragIconBoundsInScreenForTest());

  // Verifies that:
  // (1) Scrollable shelf view has the expected layout strategy.
  // (2) The dragged view has the correct view index.
  EXPECT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  const auto view_index = view_model->GetIndexOfView(dragged_view);
  EXPECT_GE(view_index,
            scrollable_shelf_view_->first_tappable_app_index().value());
  EXPECT_LE(view_index,
            scrollable_shelf_view_->last_tappable_app_index().value());
}

// Verifies that after adding the second display, shelf icons showing on
// the primary display are also visible on the second display
// (https://crbug.com/1035596).
TEST_P(ScrollableShelfViewRTLTest, CheckTappableIndicesOnSecondDisplay) {
  constexpr size_t icon_number = 5;
  for (size_t i = 0; i < icon_number; i++) {
    AddAppShortcut();
  }

  // Adds the second display.
  UpdateDisplay("600x800,600x800");

  Shelf* secondary_shelf =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
          ->shelf();
  ScrollableShelfView* secondary_scrollable_shelf_view =
      secondary_shelf->shelf_widget()
          ->hotseat_widget()
          ->scrollable_shelf_view();

  // Verifies that the all icons are visible on the secondary display.
  EXPECT_EQ(icon_number - 1,
            secondary_scrollable_shelf_view->last_tappable_app_index().value());
  EXPECT_EQ(
      0u, secondary_scrollable_shelf_view->first_tappable_app_index().value());
}

// Verifies that the scrollable shelf in oveflow mode has the correct layout
// after switching to tablet mode (https://crbug.com/1017979).
TEST_P(ScrollableShelfViewRTLTest, CorrectUIAfterSwitchingToTablet) {
  // Add enough app shortcuts to ensure that at least three pages of icons show.
  for (int i = 0; i < 25; i++) {
    AddAppShortcut();
  }
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Calculate the hotseat background's screen bounds.
  gfx::Rect hotseat_background =
      scrollable_shelf_view_->GetHotseatBackgroundBounds();
  views::View::ConvertRectToScreen(scrollable_shelf_view_, &hotseat_background);

  // Verify that the right arrow button has the correct end padding.
  const gfx::Rect right_arrow_bounds =
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen();
  if (base::i18n::IsRTL()) {
    EXPECT_EQ(scrollable_shelf_constants::kArrowButtonEndPadding,
              right_arrow_bounds.x() - hotseat_background.x());
  } else {
    EXPECT_EQ(scrollable_shelf_constants::kArrowButtonEndPadding,
              hotseat_background.right() - right_arrow_bounds.right());
  }

  GetEventGenerator()->GestureTapAt(
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen().CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());

  ash::TabletModeControllerTestApi().EnterTabletMode();
  base::RunLoop().RunUntilIdle();

  views::ViewModel* view_model = shelf_view_->view_model_for_test();
  views::View* first_tappable_view = view_model->view_at(
      scrollable_shelf_view_->first_tappable_app_index().value());

  // Verifies that the gap between the left arrow button and the first tappable
  // icon is expected.
  const gfx::Rect left_arrow_bounds =
      scrollable_shelf_view_->left_arrow()->GetBoundsInScreen();

  // Activate a shelf icon's ink drop. Verify that no crash happens.
  auto* ink_drop = views::InkDrop::Get(test_api_->GetButton(0))->GetInkDrop();
  ink_drop->SnapToActivated();
  EXPECT_EQ(views::InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  if (base::i18n::IsRTL()) {
    EXPECT_EQ(left_arrow_bounds.x() -
                  scrollable_shelf_constants::kDistanceToArrowButton,
              first_tappable_view->GetBoundsInScreen().right());
  } else {
    EXPECT_EQ(left_arrow_bounds.right() +
                  scrollable_shelf_constants::kDistanceToArrowButton,
              first_tappable_view->GetBoundsInScreen().x());
  }

  VerifyTappableAppIconsUnderMouseHover();
}

// Verifies that the scrollable shelf without overflow has the correct layout in
// tablet mode.
TEST_P(ScrollableShelfViewRTLTest, CorrectUIInTabletWithoutOverflow) {
  ash::TabletModeControllerTestApi().EnterTabletMode();

  for (int i = 0; i < 3; i++) {
    AddAppShortcut();
  }
  ASSERT_EQ(ScrollableShelfView::kNotShowArrowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());

  gfx::Rect hotseat_background =
      scrollable_shelf_view_->GetHotseatBackgroundBounds();
  views::View::ConvertRectToScreen(scrollable_shelf_view_, &hotseat_background);

  views::ViewModel* view_model = shelf_view_->view_model_for_test();
  const gfx::Rect first_tappable_icon_bounds =
      view_model
          ->view_at(scrollable_shelf_view_->first_tappable_app_index().value())
          ->GetBoundsInScreen();
  const gfx::Rect last_tappable_icon_bounds =
      view_model
          ->view_at(scrollable_shelf_view_->last_tappable_app_index().value())
          ->GetBoundsInScreen();

  if (base::i18n::IsRTL()) {
    EXPECT_EQ(hotseat_background.x() + scrollable_shelf_constants::kEndPadding,
              last_tappable_icon_bounds.x());
    EXPECT_EQ(
        hotseat_background.right() - scrollable_shelf_constants::kEndPadding,
        first_tappable_icon_bounds.right());
  } else {
    EXPECT_EQ(hotseat_background.x() + scrollable_shelf_constants::kEndPadding,
              first_tappable_icon_bounds.x());
    EXPECT_EQ(
        hotseat_background.right() - scrollable_shelf_constants::kEndPadding,
        last_tappable_icon_bounds.right());
  }

  VerifyTappableAppIconsUnderMouseHover();
}

// Verifies that activating a shelf icon's ripple ring does not bring crash
// on an extremely small display. It is an edge case detected by fuzz tests.
TEST_P(ScrollableShelfViewRTLTest, VerifyActivateIconRippleOnVerySmallDisplay) {
  AddAppShortcut();

  // Resize the display to ensure that no shelf icon is visible.
  UpdateDisplay("60x601");

  // Activate a shelf icon's ink drop. Verify that no crash happens.
  auto* ink_drop = views::InkDrop::Get(test_api_->GetButton(0))->GetInkDrop();
  ink_drop->SnapToActivated();
  EXPECT_EQ(views::InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());
}

// Verifies that the scrollable shelf without overflow has the correct layout in
// tablet mode.
TEST_P(ScrollableShelfViewRTLTest, CheckRoundedCornersSetForInkDrop) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  ASSERT_TRUE(scrollable_shelf_view_->shelf_container_view()
                  ->layer()
                  ->rounded_corner_radii()
                  .IsEmpty());

  ShelfViewTestAPI shelf_view_test_api(shelf_view_);

  ShelfAppButton* first_icon = shelf_view_test_api.GetButton(
      scrollable_shelf_view_->first_tappable_app_index().value());
  ShelfAppButton* last_icon = shelf_view_test_api.GetButton(
      scrollable_shelf_view_->last_tappable_app_index().value());

  // When the right arrow is showing, check rounded corners are set if the ink
  // drop is visible for the first visible app.
  EXPECT_TRUE(HasRoundedCornersOnAppButtonAfterMouseRightClick(first_icon));

  // When the right arrow is showing, check rounded corners are not set if the
  // ink drop is visible for the last visible app
  EXPECT_FALSE(HasRoundedCornersOnAppButtonAfterMouseRightClick(last_icon));

  // Tap right arrow. Hotseat layout must now show left arrow.
  gfx::Rect right_arrow =
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen();
  GetEventGenerator()->GestureTapAt(right_arrow.CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Recalculate first and last icons.
  first_icon = shelf_view_test_api.GetButton(
      scrollable_shelf_view_->first_tappable_app_index().value());
  last_icon = shelf_view_test_api.GetButton(
      scrollable_shelf_view_->last_tappable_app_index().value());

  // When the left arrow is showing, check rounded corners are set if the ink
  // drop is visible for the last visible app.
  EXPECT_TRUE(HasRoundedCornersOnAppButtonAfterMouseRightClick(last_icon));

  // When the left arrow is showing, check rounded corners are not set if the
  // ink drop is visible for the first visible app
  EXPECT_FALSE(HasRoundedCornersOnAppButtonAfterMouseRightClick(first_icon));
}

// Verify that the rounded corners work as expected after transition from
// clamshell mode to tablet mode (https://crbug.com/1086484).
TEST_P(ScrollableShelfViewRTLTest,
       CheckRoundedCornersAfterTabletStateTransition) {
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  PopulateAppShortcut(1);

  ShelfAppButton* icon = test_api_->GetButton(0);

  // Right click on the app button to activate the ripple ring.
  GetEventGenerator()->MoveMouseTo(icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickRightButton();
  {
    InkDropAnimationWaiter waiter(icon);
    waiter.Wait();
  }
  ASSERT_EQ(views::InkDropState::ACTIVATED,
            views::InkDrop::Get(icon)->GetInkDrop()->GetTargetInkDropState());

  // Verify that in clamshell when the ripple ring is activated, the rounded
  // corners should not be applied.
  EXPECT_TRUE(
      scrollable_shelf_view_->IsAnyCornerButtonInkDropActivatedForTest());
  EXPECT_TRUE(scrollable_shelf_view_->shelf_container_view()
                  ->layer()
                  ->rounded_corner_radii()
                  .IsEmpty());

  // Switch to tablet mode. The ripple ring should be hidden.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            views::InkDrop::Get(icon)->GetInkDrop()->GetTargetInkDropState());

  // Verify that the rounded corners should not be applied when the ripple ring
  // is hidden.
  ASSERT_TRUE(scrollable_shelf_view_->shelf_container_view()
                  ->layer()
                  ->rounded_corner_radii()
                  .IsEmpty());
  EXPECT_FALSE(
      scrollable_shelf_view_->IsAnyCornerButtonInkDropActivatedForTest());
}

// Verify that the count of activated corner buttons is expected after removing
// an app icon from context menu (https://crbug.com/1086484).
TEST_F(ScrollableShelfViewTest,
       CheckRoundedCornersAfterUnpinningFromContextMenu) {
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ash::TabletModeControllerTestApi().EnterTabletMode();

  AddAppShortcut();
  const ShelfID app_id = AddAppShortcut();
  ASSERT_EQ(2u, test_api_->GetButtonCount());

  ShelfModel* shelf_model = ShelfModel::Get();
  const int index = shelf_model->ItemIndexByID(app_id);

  ShelfAppButton* icon = test_api_->GetButton(index);

  // Right click on the app button to activate the ripple ring.
  GetEventGenerator()->MoveMouseTo(icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickRightButton();
  {
    InkDropAnimationWaiter waiter(icon);
    waiter.Wait();
  }
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            views::InkDrop::Get(icon)->GetInkDrop()->GetTargetInkDropState());

  // Emulate to remove a shelf icon from context menu.
  shelf_model->RemoveItemAt(index);
  test_api_->RunMessageLoopUntilAnimationsDone();
  ASSERT_EQ(1u, test_api_->GetButtonCount());

  // Verify the count of activated corner buttons.
  EXPECT_FALSE(
      scrollable_shelf_view_->IsAnyCornerButtonInkDropActivatedForTest());
}

// Verifies that when two shelf app buttons are animating at the same time,
// rounded corners are being kept if needed. (see https://crbug.com/1079330)
TEST_F(ScrollableShelfViewTest, CheckRoundedCornersAfterLongPress) {
  // Enable animations so that we can make sure that they occur.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ash::TabletModeControllerTestApi().EnterTabletMode();
  PopulateAppShortcut(3);
  ASSERT_EQ(ScrollableShelfView::kNotShowArrowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());
  ASSERT_TRUE(scrollable_shelf_view_->shelf_container_view()
                  ->layer()
                  ->rounded_corner_radii()
                  .IsEmpty());
  ui::Layer* layer = scrollable_shelf_view_->shelf_container_view()->layer();
  ShelfAppButton* first_icon = test_api_->GetButton(0);
  ShelfAppButton* last_icon = test_api_->GetButton(2);

  // Trigger the ripple animation over the leftmost icon and wait for it to
  // finish. Rounded corners should be set.
  GetEventGenerator()->MoveMouseTo(
      first_icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickRightButton();
  {
    InkDropAnimationWaiter waiter(first_icon);
    waiter.Wait();
  }

  ASSERT_EQ(first_icon->GetInkDropForTesting()->GetTargetInkDropState(),
            views::InkDropState::ACTIVATED);
  // The gfx::RoundedCornersF object is considered empty when all of the
  // corners are squared (no effective radius).
  EXPECT_FALSE(layer->rounded_corner_radii().IsEmpty());

  // While ripple is showing on the leftmost icon, trigger the ripple animation
  // over the rightmost icon and wait for it to finish. Rounded corners should
  // be set.
  GetEventGenerator()->MoveMouseTo(
      last_icon->GetBoundsInScreen().CenterPoint());
  // Click once so the ripple on the leftmost icon will animate to hide.
  // Immediately click again to trigger the rightmost icon animation to show.
  GetEventGenerator()->ClickRightButton();
  GetEventGenerator()->ClickRightButton();
  {
    InkDropAnimationWaiter waiter(last_icon);
    waiter.Wait();
  }

  ASSERT_EQ(first_icon->GetInkDropForTesting()->GetTargetInkDropState(),
            views::InkDropState::HIDDEN);
  ASSERT_EQ(last_icon->GetInkDropForTesting()->GetTargetInkDropState(),
            views::InkDropState::ACTIVATED);
  EXPECT_FALSE(layer->rounded_corner_radii().IsEmpty());
}

// Verifies that doing a mousewheel scroll on the scrollable shelf does scroll
// forward.
TEST_P(ScrollableShelfViewRTLTest, ScrollWithMouseWheel) {
  // The scroll threshold. Taken from |kScrollOffsetThreshold| in
  // scrollable_shelf_view.cc.
  constexpr int scroll_threshold = 20;
  AddAppShortcutsUntilOverflow();

  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Do a mousewheel scroll with a positive offset bigger than the scroll
  // threshold to scroll forward. Unlike touchpad scrolls, mousewheel scrolls
  // can only be along the cross axis.
  GetEventGenerator()->MoveMouseTo(
      scrollable_shelf_view_->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->MoveMouseWheel(0, -(scroll_threshold + 1));
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Do a mousewheel scroll with a negative offset bigger than the scroll
  // threshold to scroll backwards.
  GetEventGenerator()->MoveMouseWheel(0, scroll_threshold + 1);
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Do a mousewheel scroll with an offset smaller than the scroll
  // threshold should be ignored.
  GetEventGenerator()->MoveMouseWheel(0, scroll_threshold);
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  GetEventGenerator()->MoveMouseWheel(0, -scroll_threshold);
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
}

// Verifies that removing a shelf icon by mouse works as expected on scrollable
// shelf (see https://crbug.com/1033967).
TEST_P(ScrollableShelfViewRTLTest, RipOffShelfItem) {
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  views::ViewModel* view_model = shelf_view_->view_model_for_test();
  const gfx::Rect first_tappable_view_bounds =
      view_model
          ->view_at(scrollable_shelf_view_->first_tappable_app_index().value())
          ->GetBoundsInScreen();

  const gfx::Point drag_start_location =
      first_tappable_view_bounds.CenterPoint();
  const gfx::Point drag_end_location = gfx::Point(
      drag_start_location.x(),
      drag_start_location.y() - 3 * ShelfConfig::Get()->shelf_size());

  // Drags an icon off the shelf to remove it.
  GetEventGenerator()->MoveMouseTo(drag_start_location);
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(drag_end_location);
  GetEventGenerator()->ReleaseLeftButton();

  // Expects that the scrollable shelf has the correct layout strategy.
  EXPECT_EQ(ScrollableShelfView::kNotShowArrowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());
}

TEST_P(ScrollableShelfViewRTLTest, ScrollsByMouseWheelEvent) {
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  GetEventGenerator()->MoveMouseTo(
      scrollable_shelf_view_->GetBoundsInScreen().CenterPoint());
  constexpr int scroll_threshold =
      scrollable_shelf_constants::kScrollOffsetThreshold;

  // Verifies that it should not scroll the shelf backward anymore if the layout
  // strategy is kShowRightArrowButton.
  GetEventGenerator()->MoveMouseWheel(scroll_threshold + 1, 0);
  EXPECT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Verifies that the mouse wheel event with the offset smaller than the
  // threshold should be ignored.
  GetEventGenerator()->MoveMouseWheel(-scroll_threshold + 1, 0);
  EXPECT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  GetEventGenerator()->MoveMouseWheel(-scroll_threshold - 1, 0);
  EXPECT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
}

// Verifies that the scrollable shelf handles the scroll event (usually
// generated by the touchpad scroll) as expected.
TEST_P(ScrollableShelfViewRTLTest, VerifyScrollEvent) {
  AddAppShortcutsUntilOverflow();

  // Checks the default state of the scrollable shelf and the launcher.
  constexpr ScrollableShelfView::LayoutStrategy default_strategy =
      ScrollableShelfView::kShowRightArrowButton;
  ASSERT_EQ(default_strategy,
            scrollable_shelf_view_->layout_strategy_for_test());

  const gfx::Point start_point =
      scrollable_shelf_view_->GetBoundsInScreen().CenterPoint();
  constexpr int scroll_steps = 1;
  constexpr int num_fingers = 2;

  // Sufficient speed to exceed the threshold.
  constexpr int scroll_speed = 50;

  // Verifies that scrolling horizontally should be handled by the
  // scrollable shelf.
  GetEventGenerator()->ScrollSequence(start_point, base::TimeDelta(),
                                      -scroll_speed, /*y_offset*/ 0,
                                      scroll_steps, num_fingers);
  EXPECT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
}

// Verify that the ripple ring of the first/last app icon is fully shown
// (https://crbug.com/1057710).
TEST_P(ScrollableShelfViewRTLTest, CheckInkDropRippleOfEdgeIcons) {
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  ShelfViewTestAPI shelf_view_test_api(shelf_view_);
  ShelfAppButton* first_app_button = shelf_view_test_api.GetButton(
      scrollable_shelf_view_->first_tappable_app_index().value());
  VeirifyRippleRingWithinShelfContainer(*first_app_button);

  // Tap at the right arrow. Hotseat layout should show the left arrow.
  gfx::Rect right_arrow =
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen();
  GetEventGenerator()->GestureTapAt(right_arrow.CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  ShelfAppButton* last_app_button = shelf_view_test_api.GetButton(
      scrollable_shelf_view_->last_tappable_app_index().value());
  VeirifyRippleRingWithinShelfContainer(*last_app_button);
}

// Verifies that right-click on the last shelf icon should open the icon's
// context menu instead of the shelf's (https://crbug.com/1041702).
TEST_P(ScrollableShelfViewRTLTest, ClickAtLastIcon) {
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Taps at the right arrow. Hotseat layout should show the left arrow.
  gfx::Rect right_arrow =
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen();
  GetEventGenerator()->GestureTapAt(right_arrow.CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Right-click on the edge of the last icon.
  const views::View* last_icon = shelf_view_->view_model_for_test()->view_at(
      scrollable_shelf_view_->last_tappable_app_index().value());
  gfx::Point click_point = last_icon->GetBoundsInScreen().right_center();
  click_point.Offset(-1, 0);
  GetEventGenerator()->MoveMouseTo(click_point);
  GetEventGenerator()->ClickRightButton();

  // Verifies that the context menu of |last_icon| should show.
  EXPECT_TRUE(shelf_view_->IsShowingMenuForView(last_icon));

  // Verfies that after left-click, the context menu should be closed.
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(shelf_view_->IsShowingMenuForView(last_icon));
}

// Verifies that mouse click at the second last shelf item during the last item
// removal animation does not lead to crash (see https://crbug.com/1300561).
TEST_F(ScrollableShelfViewTest, RemoveLastItemWhileClickingSeoncdLastOne) {
  PopulateAppShortcut(3);
  ASSERT_EQ(ScrollableShelfView::kNotShowArrowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());

  const int view_size_before_removal =
      shelf_view_->view_model_for_test()->view_size();
  {
    // Remove the last shelf item with animation enabled.
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
    ShelfModel::Get()->RemoveItemAt(view_size_before_removal - 1);
    EXPECT_TRUE(shelf_view_->IsAnimating());
  }

  // Mouse right click at the second last item and wait for the ink drop
  // animation to complete.
  ShelfAppButton* second_last_item =
      ShelfViewTestAPI(shelf_view_).GetButton(view_size_before_removal - 2);
  GetEventGenerator()->MoveMouseTo(
      second_last_item->GetBoundsInScreen().CenterPoint());
  InkDropAnimationWaiter waiter(second_last_item);
  GetEventGenerator()->ClickRightButton();
  waiter.Wait();
}

// Verifies that presentation time for shelf gesture scroll is recorded as
// expected (https://crbug.com/1095259).
TEST_F(ScrollableShelfViewTest, PresentationTimeMetricsForGestureScroll) {
  ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
      true);

  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  views::ViewModel* view_model = shelf_view_->view_model_for_test();
  ASSERT_GT(view_model->view_size(), 2u);

  // |gesture_start_point| is between the first and the second shelf icon. It
  // ensures that gesture scroll does not start from a point within any shelf
  // icon's bounds.
  const gfx::Point first_icon_center =
      view_model->view_at(0)->GetBoundsInScreen().CenterPoint();
  const gfx::Point second_icon_center =
      view_model->view_at(1)->GetBoundsInScreen().CenterPoint();
  ASSERT_EQ(first_icon_center.y(), second_icon_center.y());
  const gfx::Point gesture_start_point(
      (first_icon_center.x() + second_icon_center.x()) / 2,
      first_icon_center.y());

  GetEventGenerator()->PressTouch(gesture_start_point);

  base::HistogramTester histogram_tester;
  auto check_bucket_size = [&histogram_tester](
                               int presentation_time_expected_size,
                               int max_latency_expected_size) {
    histogram_tester.ExpectTotalCount(
        "Apps.ScrollableShelf.Drag.PresentationTime.ClamshellMode."
        "LauncherHidden",
        presentation_time_expected_size);
    histogram_tester.ExpectTotalCount(
        "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.ClamshellMode."
        "LauncherHidden",
        max_latency_expected_size);
  };

  int last_scroll_distance = 0;
  ScrollableShelfView* const scrollable_shelf_view = scrollable_shelf_view_;

  // Helper function to update |last_scroll_distance| and check whether shelf
  // is scrolled successfully.
  auto shelf_scrolled = [&last_scroll_distance,
                         scrollable_shelf_view]() -> bool {
    const int current_scroll_distance =
        scrollable_shelf_view->scroll_offset_for_test().x();
    const bool scrolled = last_scroll_distance != current_scroll_distance;
    last_scroll_distance = current_scroll_distance;
    return scrolled;
  };

  // Scroll the shelf leftward. Because scrollable shelf already reaches the
  // left end. So shelf should not be scrolled successfully and the bucket
  // number should not change.
  GetEventGenerator()->MoveTouchBy(10, 0);
  ASSERT_EQ(0, last_scroll_distance);
  EXPECT_FALSE(shelf_scrolled());
  check_bucket_size(0, 0);

  // Scroll the shelf rightward. Verify that shelf should be scrolled to the
  // right end. The bucket number changes as expected.
  GetEventGenerator()->MoveTouchBy(
      -scrollable_shelf_view_->GetScrollUpperBoundForTest() - 5, 0);
  EXPECT_TRUE(shelf_scrolled());
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());
  check_bucket_size(1, 0);

  // Scroll the shelf rightward. Because scrollable shelf already reaches the
  // right end. So shelf should not be scrolled successfully and the bucket
  // number should not change.
  GetEventGenerator()->MoveTouchBy(-10, 0);
  EXPECT_FALSE(shelf_scrolled());
  check_bucket_size(1, 0);

  // End gesture. Verify that the max latency is recorded.
  GetEventGenerator()->ReleaseTouch();
  check_bucket_size(1, 1);
}

// Verifies that the shelf is scrolled to show the pinned app after pinning.
TEST_P(ScrollableShelfViewRTLTest, FeedbackForAppPinning) {
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // |num| is the minimum of app icons to enter overflow mode.
  const size_t num = shelf_view_->view_model_for_test()->view_size();

  ShelfModel::ScopedUserTriggeredMutation user_triggered(
      scrollable_shelf_view_->shelf_view()->model());

  {
    ShelfID shelf_id = AddAppShortcut();
    const size_t view_index =
        shelf_view_->model()->ItemIndexByAppID(shelf_id.app_id);
    ASSERT_EQ(view_index, num);

    // When shelf only contains pinned apps, expects that the shelf is scrolled
    // to the last page to show the latest pinned app.
    EXPECT_EQ(ScrollableShelfView::kShowLeftArrowButton,
              scrollable_shelf_view_->layout_strategy_for_test());
  }

  GetEventGenerator()->GestureTapAt(
      scrollable_shelf_view_->left_arrow()->GetBoundsInScreen().CenterPoint());
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Pins the icons of running apps to the shelf.
  for (size_t i = 0; i < 2 * num; i++) {
    AddAppShortcut(ShelfItemType::TYPE_APP);
  }

  {
    ShelfID shelf_id = AddAppShortcut();
    const size_t view_index =
        shelf_view_->model()->ItemIndexByAppID(shelf_id.app_id);
    ASSERT_EQ(view_index, num + 1);

    // Scrolls the shelf to show the pinned app. Expects that the shelf is
    // scrolled to the correct page. Notes that the pinned app should be placed
    // ahead of running apps.
    EXPECT_LT(view_index,
              scrollable_shelf_view_->last_tappable_app_index().value());
    EXPECT_GT(view_index,
              scrollable_shelf_view_->first_tappable_app_index().value());
    EXPECT_EQ(ScrollableShelfView::kShowButtons,
              scrollable_shelf_view_->layout_strategy_for_test());
  }
}

class ScrollableShelfViewWithAppScalingTest : public ScrollableShelfViewTest {
 public:
  ScrollableShelfViewWithAppScalingTest() = default;
  ~ScrollableShelfViewWithAppScalingTest() override = default;

  void SetUp() override {
    // With the calendar view, the status widget's bounds could vary under
    // different dates. For example, "June 10" is longer than "June 9".
    // Therefore, the code below sets the constant date to avoid flakiness.

    scoped_locale_ =
        std::make_unique<base::test::ScopedRestoreICUDefaultLocale>("en_US"),
    time_zone_ = std::make_unique<base::test::ScopedRestoreDefaultTimezone>(
        "America/Chicago");

    constexpr char kFakeNowTimeString[] = "Sunday, 5 June 2022 14:30:00 CDT";
    ASSERT_TRUE(base::Time::FromString(kFakeNowTimeString,
                                       &TimeOverrideHelper::current_time));
    time_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        &TimeOverrideHelper::TimeNow, /*time_ticks_override=*/nullptr,
        /*thread_ticks_override=*/nullptr);

    ScrollableShelfViewTest::SetUp();

    // Display should be big enough (width and height are bigger than 600).
    // Otherwise, shelf is in dense mode by default.
    // Note that the display width is hard coded. The display width should
    // ensure that the two-stage scaling is possible to happen. Otherwise,
    // there may be insufficient space to accommodate shelf icons in
    // |ShelfConfig::shelf_button_mediate_size_| then hotseat density may switch
    // from kNormal to kDense directly.
    UpdateDisplay("820x601");

    // App scaling is only used in tablet mode.
    ash::TabletModeControllerTestApi().EnterTabletMode();
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(ShelfConfig::Get()->is_dense());
  }

 protected:
  // |kAppCount| is a magic number, which satisfies the following
  // conditions:
  // (1) Scrollable shelf shows |kAppCount| app buttons in size of
  // |ShelfConfig::shelf_button_size_| without scrolling.
  // (2) Scrollable shelf shows (|kAppCount| + 1) app buttons in size of
  // |ShelfConfig::shelf_button_mediate_size_| without scrolling.
  // (3) Scrollable shelf cannot show (|kAppCount| + 2) app buttons in size of
  // |ShelfConfig::shelf_button_mediate_size_| without scrolling.

  // Addition or removal of shelf button should not change hotseat state.
  // So Hotseat widget's width is a constant. Then |kAppCount| is in the range
  // of [1, (hotseat width) / (shelf button + button spacing) + 1].
  // So we can get |kAppCount| in that range manually
  static constexpr int kAppCount = 10;

  // If calendar view is enabled, the space is a little smaller and can only
  // show 9 apps at one time.
  static constexpr int kAppCountWithShowingDateTray = 9;

  // Ensures that the current time is constant if the calendar view is enabled.
  std::unique_ptr<base::test::ScopedRestoreICUDefaultLocale> scoped_locale_;
  std::unique_ptr<base::test::ScopedRestoreDefaultTimezone> time_zone_;
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;
};

// Verifies the basic function of app scaling which scales down the hotseat and
// its children's sizes if there is insufficient space for shelf buttons to show
// without scrolling.
TEST_F(ScrollableShelfViewWithAppScalingTest, AppScalingBasics) {
  PopulateAppShortcut(kAppCountWithShowingDateTray);

  HotseatWidget* hotseat_widget =
      GetPrimaryShelf()->shelf_widget()->hotseat_widget();
  EXPECT_EQ(HotseatDensity::kNormal, hotseat_widget->target_hotseat_density());
  EXPECT_EQ(ScrollableShelfView::kNotShowArrowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Pin an app icon. Verify that hotseat's target density updates as expected.
  AddAppShortcut();
  EXPECT_EQ(HotseatDensity::kSemiDense,
            hotseat_widget->target_hotseat_density());
  EXPECT_EQ(ScrollableShelfView::kNotShowArrowButtons,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Pin another app icon. Verify that hotseat's target density updates as
  // expected.
  const ShelfID shelf_id = AddAppShortcut();
  EXPECT_EQ(HotseatDensity::kDense, hotseat_widget->target_hotseat_density());

  // Unpin an app icon.
  ShelfModel* shelf_model = ShelfModel::Get();
  const gfx::Rect bounds_before_unpin =
      hotseat_widget->GetWindowBoundsInScreen();
  const int shelf_button_size_before = shelf_view_->GetButtonSize();
  shelf_model->RemoveItemAt(shelf_model->ItemIndexByID(shelf_id));
  test_api_->RunMessageLoopUntilAnimationsDone();

  // Verify that:
  // (1) After unpinning, hotseat's target density is expected.
  // (2) Hotseat widget's size and the shelf button size are expected.
  const gfx::Rect bounds_after_unpin =
      hotseat_widget->GetWindowBoundsInScreen();
  const int shelf_button_size_after = shelf_view_->GetButtonSize();
  EXPECT_EQ(HotseatDensity::kSemiDense,
            hotseat_widget->target_hotseat_density());
  EXPECT_EQ(bounds_before_unpin.width(), bounds_after_unpin.width());
  EXPECT_LT(bounds_before_unpin.height(), bounds_after_unpin.height());
  EXPECT_LT(shelf_button_size_before, shelf_button_size_after);
}

// Verifies that app scaling works as expected with hotseat state transition.
TEST_F(ScrollableShelfViewWithAppScalingTest,
       VerifyWithHotseatStateTransition) {
  PopulateAppShortcut(kAppCountWithShowingDateTray);

  HotseatWidget* hotseat_widget =
      GetPrimaryShelf()->shelf_widget()->hotseat_widget();
  EXPECT_EQ(HotseatDensity::kNormal, hotseat_widget->target_hotseat_density());

  // Pin an app icon then enter the overview mode. Verify that app scaling is
  // turned on.
  const ShelfID shelf_id = AddAppShortcut();
  EnterOverview();
  WaitForOverviewAnimation(/*enter=*/true);
  EXPECT_EQ(HotseatDensity::kSemiDense,
            hotseat_widget->target_hotseat_density());

  // Unpin an app icon. Verify that hotseat density updates.
  ShelfModel* shelf_model = ShelfModel::Get();
  shelf_model->RemoveItemAt(shelf_model->ItemIndexByID(shelf_id));
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(HotseatDensity::kNormal, hotseat_widget->target_hotseat_density());

  // Exit overview mode. Verify the hotseat density.
  ExitOverview();
  WaitForOverviewAnimation(/*enter=*/false);
  EXPECT_EQ(HotseatDensity::kNormal, hotseat_widget->target_hotseat_density());
}

TEST_F(ScrollableShelfViewWithAppScalingTest, TabletModeTransition) {
  PopulateAppShortcut(kAppCountWithShowingDateTray);

  HotseatWidget* hotseat_widget =
      GetPrimaryShelf()->shelf_widget()->hotseat_widget();
  EXPECT_EQ(HotseatDensity::kNormal, hotseat_widget->target_hotseat_density());

  // Pin an app icon and verify that app scaling is turned on.
  const ShelfID shelf_id = AddAppShortcut();
  EXPECT_EQ(HotseatDensity::kSemiDense,
            hotseat_widget->target_hotseat_density());

  // Switch to clamshell and verify that hotseat density reverts to normal.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(HotseatDensity::kNormal, hotseat_widget->target_hotseat_density());

  // Go back to tablet mode, and verify density gets updated.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(HotseatDensity::kSemiDense,
            hotseat_widget->target_hotseat_density());
}

TEST_F(ScrollableShelfViewWithAppScalingTest,
       TabletModeTransitionWithVerticalShelf) {
  PrefService* const prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  SetShelfAlignmentPref(prefs, GetPrimaryDisplay().id(), ShelfAlignment::kLeft);

  PopulateAppShortcut(kAppCountWithShowingDateTray);
  HotseatWidget* hotseat_widget =
      GetPrimaryShelf()->shelf_widget()->hotseat_widget();
  EXPECT_EQ(HotseatDensity::kNormal, hotseat_widget->target_hotseat_density());

  // Pin an app icon and verify that app scaling is turned on.
  const ShelfID shelf_id = AddAppShortcut();
  EXPECT_EQ(HotseatDensity::kSemiDense,
            hotseat_widget->target_hotseat_density());

  // Switch to clamshell and verify that hotseat density reverts to normal.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(HotseatDensity::kNormal, hotseat_widget->target_hotseat_density());

  // Go back to tablet mode, and verify density gets updated.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(HotseatDensity::kSemiDense,
            hotseat_widget->target_hotseat_density());
}

// Verifies that right-click on scroll arrows shows shelf's context menu
// (https://crbug.com/1324741).
TEST_F(ScrollableShelfViewTest, RightClickArrows) {
  AddAppShortcutsUntilOverflow();
  ASSERT_EQ(ScrollableShelfView::kShowRightArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Right click on the right arrow. Shelf context menu should show.
  gfx::Rect right_arrow =
      scrollable_shelf_view_->right_arrow()->GetBoundsInScreen();
  GetEventGenerator()->MoveMouseTo(right_arrow.CenterPoint());
  GetEventGenerator()->ClickRightButton();

  EXPECT_TRUE(
      shelf_view_->IsShowingMenuForView(scrollable_shelf_view_->right_arrow()));

  // Now click on the right arrow. Hotseat layout should show the left arrow.
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(
      shelf_view_->IsShowingMenuForView(scrollable_shelf_view_->right_arrow()));
  GetEventGenerator()->ClickLeftButton();
  ASSERT_EQ(ScrollableShelfView::kShowLeftArrowButton,
            scrollable_shelf_view_->layout_strategy_for_test());

  // Right-click on the left arrow. Shelf context menu should show.
  gfx::Rect left_arrow =
      scrollable_shelf_view_->left_arrow()->GetBoundsInScreen();
  GetEventGenerator()->MoveMouseTo(left_arrow.CenterPoint());
  GetEventGenerator()->ClickRightButton();

  EXPECT_TRUE(
      shelf_view_->IsShowingMenuForView(scrollable_shelf_view_->left_arrow()));

  // After left-click, the context menu should be closed.
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(
      shelf_view_->IsShowingMenuForView(scrollable_shelf_view_->left_arrow()));
}

// Verifies that activating an app will automatically scroll the shelf to show
// the activated app icon.
TEST_P(ScrollableShelfViewRTLTest, ActivateAppScrollShelfToMakeAppVisible) {
  AddAppShortcutsUntilOverflow();
  const ShelfButton* first_button = test_api_->GetButton(0);
  const int last_button_index = test_api_->GetButtonCount() - 1;
  const ShelfButton* last_button = test_api_->GetButton(last_button_index);
  gfx::Rect visible_space_in_screen = scrollable_shelf_view_->visible_space();
  views::View::ConvertRectToScreen(scrollable_shelf_view_,
                                   &visible_space_in_screen);

  // Initially, the first button will show while the last one will not because
  // of the overflow.
  EXPECT_TRUE(
      visible_space_in_screen.Contains(first_button->GetBoundsInScreen()));
  EXPECT_FALSE(
      visible_space_in_screen.Contains(last_button->GetBoundsInScreen()));

  gfx::Rect first_window_bounds(0, 0, 200, 200);
  gfx::Rect last_window_bounds(200, 0, 200, 200);

  // Create new windows for the first and last apps on the shelf. Set the
  // `kShelfIDKey` property to associate windows with shelf apps.
  std::unique_ptr<aura::Window> first_app_window =
      CreateTestWindow(first_window_bounds);
  first_app_window->SetProperty(kShelfIDKey,
                                ShelfModel::Get()->items()[0].id.Serialize());
  std::unique_ptr<aura::Window> last_app_window =
      CreateTestWindow(last_window_bounds);
  last_app_window->SetProperty(
      kShelfIDKey,
      ShelfModel::Get()->items()[last_button_index].id.Serialize());

  // Activate `last_app_window` by mouse click. It emulates the process that an
  // app window is activated after clicking at a shelf app icon.
  GetEventGenerator()->MoveMouseTo(last_window_bounds.CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // Get the new visible space of `scrollable_shelf_view_`
  visible_space_in_screen = scrollable_shelf_view_->visible_space();
  views::View::ConvertRectToScreen(scrollable_shelf_view_,
                                   &visible_space_in_screen);

  // The last app button should be showing while the first app button is not
  // because the shelf is scrolled.
  EXPECT_FALSE(
      visible_space_in_screen.Contains(first_button->GetBoundsInScreen()));
  EXPECT_TRUE(
      visible_space_in_screen.Contains(last_button->GetBoundsInScreen()));

  // Activate `last_app_window` by mouse click. It emulates the process that an
  // app window is activated after clicking at a shelf app icon.
  GetEventGenerator()->MoveMouseTo(first_window_bounds.CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // Get the new visible space of `scrollable_shelf_view_`
  visible_space_in_screen = scrollable_shelf_view_->visible_space();
  views::View::ConvertRectToScreen(scrollable_shelf_view_,
                                   &visible_space_in_screen);

  // The first app button should be showing while the last app button is not
  // because the shelf is scrolled.
  EXPECT_TRUE(
      visible_space_in_screen.Contains(first_button->GetBoundsInScreen()));
  EXPECT_FALSE(
      visible_space_in_screen.Contains(last_button->GetBoundsInScreen()));
}

namespace {

class ScrollableShelfViewDeskButtonTest : public ScrollableShelfViewTest {
 public:
  ScrollableShelfViewDeskButtonTest() = default;
  ScrollableShelfViewDeskButtonTest(const ScrollableShelfViewDeskButtonTest&) =
      delete;
  ScrollableShelfViewDeskButtonTest& operator=(
      const ScrollableShelfViewDeskButtonTest&) = delete;
  ~ScrollableShelfViewDeskButtonTest() override = default;

  // ScrollableShelfViewTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kDeskButton,
                              chromeos::features::kJellyroll},
        /*disabled_features=*/{});

    // Shelf overflow can be influenced by system time (i.e. if the date is
    // slightly longer or the clock has four digits instead of three, this can
    // cause overflow). We set the timer to be a consistent time so that the
    // time cannot impact the test's success.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kStabilizeTimeDependentViewForTests);

    ScrollableShelfViewTest::SetUp();
    SetShowDeskButtonInShelfPref(
        Shell::Get()->session_controller()->GetPrimaryUserPrefService(), true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Verify that desk button behavior before and after shelf is overflown.
TEST_F(ScrollableShelfViewDeskButtonTest, ButtonRespondsToOverflowStateChange) {
  SetShelfAnimationDuration(base::Milliseconds(1));

  auto* shelf = GetPrimaryShelf();
  auto* hotseat_widget = shelf->hotseat_widget();
  auto* scrollable_shelf_view = hotseat_widget->scrollable_shelf_view();
  auto* desk_button_widget = shelf->desk_button_widget();
  shelf->SetAlignment(ShelfAlignment::kBottom);
  EXPECT_FALSE(hotseat_widget->CalculateShelfOverflow(true));
  EXPECT_EQ(ScrollableShelfView::LayoutStrategy::kNotShowArrowButtons,
            scrollable_shelf_view->layout_strategy_for_test());

  // Keep adding apps until the shelf overflows. The desk button should remain
  // expanded.
  ShelfID last_app_id;
  gfx::Rect last_desk_button_bounds;
  for (int i = 0; i < 50 && !hotseat_widget->CalculateShelfOverflow(true);
       i++) {
    last_desk_button_bounds = desk_button_widget->GetTargetBounds();
    last_app_id = AddAppShortcut();
    WaitForShelfAnimation();
    ASSERT_NE(last_desk_button_bounds, desk_button_widget->GetTargetBounds());
  }
  EXPECT_TRUE(hotseat_widget->CalculateShelfOverflow(true));
  EXPECT_EQ(ScrollableShelfView::LayoutStrategy::kShowRightArrowButton,
            scrollable_shelf_view->layout_strategy_for_test());

  // Add one more app, desk button does not change its bounds. The desk button
  // remains at the same bounds.
  auto* shelf_model = ShelfModel::Get();
  last_desk_button_bounds = desk_button_widget->GetTargetBounds();
  ShelfID new_app_id = AddAppShortcut();
  WaitForShelfAnimation();
  EXPECT_EQ(last_desk_button_bounds, desk_button_widget->GetTargetBounds());
  EXPECT_TRUE(hotseat_widget->CalculateShelfOverflow(true));
  EXPECT_EQ(ScrollableShelfView::LayoutStrategy::kShowRightArrowButton,
            scrollable_shelf_view->layout_strategy_for_test());

  // Remove the new app, desk button does not change its bounds. The desk button
  // remains at the same bounds.
  shelf_model->RemoveItemAt(shelf_model->ItemIndexByID(new_app_id));
  WaitForShelfAnimation();
  EXPECT_EQ(last_desk_button_bounds, desk_button_widget->GetTargetBounds());
  EXPECT_TRUE(hotseat_widget->CalculateShelfOverflow(true));
  EXPECT_EQ(ScrollableShelfView::LayoutStrategy::kShowRightArrowButton,
            scrollable_shelf_view->layout_strategy_for_test());

  // Remove the last app icon so that the shelf does not overflow. The desk
  // button changes its bounds.
  shelf_model->RemoveItemAt(shelf_model->ItemIndexByID(last_app_id));
  WaitForShelfAnimation();
  EXPECT_NE(last_desk_button_bounds, desk_button_widget->GetTargetBounds());
  EXPECT_FALSE(hotseat_widget->CalculateShelfOverflow(true));
  EXPECT_EQ(ScrollableShelfView::LayoutStrategy::kNotShowArrowButtons,
            scrollable_shelf_view->layout_strategy_for_test());
}

}  // namespace ash
