// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_widget.h"

#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/virtual_keyboard_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/mahi/fake_mahi_manager.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ash/wm/work_area_insets.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kPanelDefaultWidth = 360;
constexpr int kPanelDefaultHeight = 492;
constexpr int kPanelBoundsShelfPadding = 8;

}  // namespace

class MahiPanelWidgetTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kMahi,
                              chromeos::features::kFeatureManagementMahi},
        /*disabled_features=*/{});
    AshTestBase::SetUp();

    fake_mahi_manager_ = std::make_unique<FakeMahiManager>();
    scoped_setter_ = std::make_unique<chromeos::ScopedMahiManagerSetter>(
        fake_mahi_manager_.get());
  }

  void TearDown() override {
    scoped_setter_.reset();
    fake_mahi_manager_.reset();

    AshTestBase::TearDown();
  }

 protected:
  MahiUiController ui_controller_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeMahiManager> fake_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
};

TEST_F(MahiPanelWidgetTest, DefaultWidgetBounds) {
  auto widget = MahiPanelWidget::CreateAndShowPanelWidget(
      GetPrimaryDisplay().id(),
      /*mahi_menu_bounds=*/gfx::Rect(10, 10, 300, 300), &ui_controller_);

  // The mahi panel should have the same origin as the mahi_menu_bounds when
  // there is enough space for it.
  EXPECT_EQ(gfx::Rect(gfx::Point(10, 10),
                      gfx::Size(kPanelDefaultWidth, kPanelDefaultHeight)),
            widget->GetRestoredBounds());
}

TEST_F(MahiPanelWidgetTest, WidgetPositionWithConstrainedBottomSpace) {
  UpdateDisplay("800x700");
  // Place the menu 200px above the screen's bottom to ensure there is not
  // enough space for the panel to align with the top of the mahi menu.
  auto widget = MahiPanelWidget::CreateAndShowPanelWidget(
      GetPrimaryDisplay().id(),
      /*mahi_menu_bounds=*/gfx::Rect(100, 500, 300, 300), &ui_controller_);

  // The panel's bottom should be `kPanelBoundsShelfPadding` pixels above the
  // work_area's bottom.
  EXPECT_EQ(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().bottom() -
          kPanelBoundsShelfPadding,
      widget->GetRestoredBounds().bottom());
}

TEST_F(MahiPanelWidgetTest, WidgetPositionAfterWorkAreaBoundsChange) {
  auto default_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  // Create a widget that has the same size as the work area and show it at the
  // bottom of the work area bounds.
  auto widget = MahiPanelWidget::CreateAndShowPanelWidget(
      GetPrimaryDisplay().id(),
      /*mahi_menu_bounds=*/
      gfx::Rect(
          gfx::Point(default_work_area.x(), default_work_area.bottom()),
          gfx::Size(default_work_area.width(), default_work_area.height())),
      &ui_controller_);

  // Reduce the user work area bounds by force-showing the virtual keyboard.
  Shell::Get()
      ->keyboard_controller()
      ->virtual_keyboard_controller()
      ->ForceShowKeyboard();
  base::RunLoop().RunUntilIdle();

  auto current_work_area = WorkAreaInsets::ForWindow(widget->GetNativeWindow())
                               ->user_work_area_bounds();
  EXPECT_LT(current_work_area.bottom(), default_work_area.bottom());

  // The panel's bottom should be `kPanelBoundsShelfPadding` pixels above the
  // work_area's bottom.
  EXPECT_EQ(current_work_area.bottom() - kPanelBoundsShelfPadding,
            widget->GetRestoredBounds().bottom());

  // Hide the virtual keyboard. The work area bounds should return to their
  // default value.
  Shell::Get()->keyboard_controller()->HideKeyboard(HideReason::kSystem);
  current_work_area = WorkAreaInsets::ForWindow(widget->GetNativeWindow())
                          ->user_work_area_bounds();
  EXPECT_EQ(current_work_area.bottom(), default_work_area.bottom());

  // The panel's top should be `kPanelBoundsShelfPadding` pixels below the
  // work_area's top.
  EXPECT_EQ(current_work_area.y() + kPanelBoundsShelfPadding,
            widget->GetRestoredBounds().y());
}

TEST_F(MahiPanelWidgetTest, WidgetPositionWithConstrainedRightSpace) {
  UpdateDisplay("800x700");

  // Place the menu at the right edge of the screen to ensure there is not
  // enough space for the panel to align with the left edge of the mahi menu.
  auto widget = MahiPanelWidget::CreateAndShowPanelWidget(
      GetPrimaryDisplay().id(),
      /*mahi_menu_bounds=*/gfx::Rect(500, 100, 300, 300), &ui_controller_);

  // The panel should be placed correctly within the work area.
  EXPECT_EQ(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().right(),
      widget->GetRestoredBounds().right());
}

TEST_F(MahiPanelWidgetTest, WidgetDestroyedDuringShowAnimation) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  auto widget = MahiPanelWidget::CreateAndShowPanelWidget(
      GetPrimaryDisplay().id(),
      /*mahi_menu_bounds=*/gfx::Rect(100, 100, 200, 200), &ui_controller_);

  ASSERT_TRUE(widget->GetContentsView()
                  ->GetViewByID(mahi_constants::ViewId::kMahiPanelView)
                  ->layer()
                  ->GetAnimator()
                  ->is_animating());

  // Expect the widget to close gracefully without a crash while an animation
  // is in progress.
  widget->CloseNow();
}

TEST_F(MahiPanelWidgetTest, WidgetBoundsAfterRefreshBannerUpdate) {
  views::UniqueWidgetPtr panel_widget =
      MahiPanelWidget::CreateAndShowPanelWidget(
          GetPrimaryDisplay().id(), /*mahi_menu_bounds=*/gfx::Rect(),
          &ui_controller_);
  // Set the widget bounds to be different to the default bounds, so that we can
  // test that the panel location is preserved.
  panel_widget->SetBounds(gfx::Rect(100, 200, 300, 200));
  const gfx::Rect kInitialPanelWidgetBounds =
      panel_widget->GetWindowBoundsInScreen();
  views::View* panel_view = panel_widget->GetContentsView()->GetViewByID(
      mahi_constants::ViewId::kMahiPanelView);
  const gfx::Rect kInitialPanelViewBounds = panel_view->GetBoundsInScreen();

  views::View* refresh_view = panel_widget->GetContentsView()->GetViewByID(
      mahi_constants::ViewId::kRefreshView);
  refresh_view->SetVisible(true);

  // The widget bounds should now provide space for the refresh banner at the
  // top, while preserving the panel view bounds.
  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen().InsetsFrom(
                kInitialPanelWidgetBounds),
            gfx::Insets::TLBR(refresh_view->height() -
                                  mahi_constants::kRefreshBannerStackDepth,
                              0, 0, 0));
  EXPECT_EQ(panel_view->GetBoundsInScreen(), kInitialPanelViewBounds);

  refresh_view->SetVisible(false);

  // The panel widget and view bounds should be restored to their values before
  // the refresh banner was shown.
  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(), kInitialPanelWidgetBounds);
  EXPECT_EQ(panel_view->GetBoundsInScreen(), kInitialPanelViewBounds);
}

// Tests that the Mahi panel widget should not resize due to a screen size
// change e.g. due to using docked magnifier.
TEST_F(MahiPanelWidgetTest, WidgetDoesNotResize) {
  // Set a window size that fits the panel and cache the widget size.
  UpdateDisplay("800x700");
  auto widget = MahiPanelWidget::CreateAndShowPanelWidget(
      GetPrimaryDisplay().id(),
      /*mahi_menu_bounds=*/gfx::Rect(gfx::Point(10, 10), gfx::Size(300, 300)),
      &ui_controller_);
  const auto panel_widget_size = widget->GetSize();

  // Reduce the screen size such that the panel widget would not entirely fit.
  // It should keep its original size.
  UpdateDisplay("200x180");
  EXPECT_EQ(widget->GetSize(), panel_widget_size);
}

// Tests that the Mahi panel widget stays visible when another window goes
// full-screen.
TEST_F(MahiPanelWidgetTest, WidgetDoesNotHideOnFullScreen) {
  // Create and show the panel widget. It should be visible.
  auto widget = MahiPanelWidget::CreateAndShowPanelWidget(
      GetPrimaryDisplay().id(),
      /*mahi_menu_bounds=*/gfx::Rect(gfx::Point(10, 10), gfx::Size(300, 300)),
      &ui_controller_);
  EXPECT_TRUE(widget->IsVisible());

  // Create a fullscreen window. The panel widget should still be visible.
  auto window = CreateTestWindow();
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  EXPECT_TRUE(widget->IsVisible());

  // Expect the mahi panel widget to be in the top-most window compared to the
  // regular window.
  EXPECT_EQ(window_util::GetTopMostWindow(
                {window->parent(), widget->GetNativeWindow()->parent()}),
            widget->GetNativeWindow()->parent());
}

// Tests that the Mahi panel widget is activatable by selecting its textfield.
TEST_F(MahiPanelWidgetTest, WidgetIsActivatable) {
  // Create and show the panel widget, it should be activatable.
  auto widget = MahiPanelWidget::CreateAndShowPanelWidget(
      GetPrimaryDisplay().id(),
      /*mahi_menu_bounds=*/gfx::Rect(gfx::Point(10, 10), gfx::Size(300, 300)),
      &ui_controller_);
  EXPECT_TRUE(widget->CanActivate());

  // Click on the textfield which should add focus to it, meaning that the
  // widget is activatable.
  auto* question_textfield = widget->GetContentsView()->GetViewByID(
      mahi_constants::ViewId::kQuestionTextfield);
  LeftClickOn(question_textfield);
  EXPECT_TRUE(question_textfield->HasFocus());
}

}  // namespace ash
