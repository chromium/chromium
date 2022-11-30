// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"

#include "ash/style/ash_color_id.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"
#include "ash/wm/window_state.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kMultitaskMenuVerticalPadding = 8;
constexpr int kBetweenButtonSpacing = 12;
constexpr int kCornerRadius = 8;
constexpr int kShadowElevation = 3;
constexpr gfx::Insets kInsideBorderInsets(16);

// The duration of the menu position animation.
constexpr base::TimeDelta kPositionAnimationDurationMs =
    base::Milliseconds(250);
// The duration of the menu opacity animation.
constexpr base::TimeDelta kOpacityAnimationDurationMs = base::Milliseconds(150);

}  // namespace

// The contents view of the multitask menu.
class TabletModeMultitaskMenuView : public views::View {
 public:
  METADATA_HEADER(TabletModeMultitaskMenuView);

  TabletModeMultitaskMenuView(aura::Window* window,
                              base::RepeatingClosure hide_menu) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshShieldAndBase80, kCornerRadius));
    SetBorder(std::make_unique<views::HighlightBorder>(
        kCornerRadius, views::HighlightBorder::Type::kHighlightBorder1,
        /*use_light_colors=*/false));

    SetUseDefaultFillLayout(true);

    // Since this menu is only shown for maximizable windows, it can be
    // fullscreened.
    // TODO(sophiewen): Ensure that there is always 2 buttons or more if this
    // view is created.
    auto* window_state = WindowState::Get(window);
    DCHECK(window_state);
    DCHECK(window_state->CanMaximize());
    uint8_t buttons = chromeos::MultitaskMenuView::kFullscreen;
    if (SplitViewController::Get(window)->CanSnapWindow(window)) {
      buttons |= chromeos::MultitaskMenuView::kHalfSplit;
      buttons |= chromeos::MultitaskMenuView::kPartialSplit;
    }
    if (chromeos::wm::CanFloatWindow(window))
      buttons |= chromeos::MultitaskMenuView::kFloat;

    multitask_menu_view_for_testing_ =
        AddChildView(std::make_unique<chromeos::MultitaskMenuView>(
            window, hide_menu, buttons));

    auto* layout = multitask_menu_view_for_testing_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, kInsideBorderInsets,
            kBetweenButtonSpacing));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
  }

  TabletModeMultitaskMenuView(const TabletModeMultitaskMenuView&) = delete;
  TabletModeMultitaskMenuView& operator=(const TabletModeMultitaskMenuView&) =
      delete;
  ~TabletModeMultitaskMenuView() override = default;

  chromeos::MultitaskMenuView* multitask_menu_view_for_testing() {
    return multitask_menu_view_for_testing_;
  }

 private:
  raw_ptr<chromeos::MultitaskMenuView> multitask_menu_view_for_testing_ =
      nullptr;
};

BEGIN_METADATA(TabletModeMultitaskMenuView, View)
END_METADATA

TabletModeMultitaskMenu::TabletModeMultitaskMenu(
    TabletModeMultitaskMenuEventHandler* event_handler,
    aura::Window* window,
    base::RepeatingClosure callback)
    : event_handler_(event_handler), window_(window) {
  // Start observing the window.
  DCHECK(window);
  observed_window_.Observe(window);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.parent = window->parent();
  params.name = "TabletModeMultitaskMenuWidget";
  params.corner_radius = kCornerRadius;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.shadow_elevation = kShadowElevation;

  multitask_menu_widget_->Init(std::move(params));
  multitask_menu_widget_->SetContentsView(
      std::make_unique<TabletModeMultitaskMenuView>(window_, callback));
  AnimateShow();

  widget_observation_.Observe(multitask_menu_widget_.get());
}

TabletModeMultitaskMenu::~TabletModeMultitaskMenu() = default;

void TabletModeMultitaskMenu::AnimateShow() {
  DCHECK(multitask_menu_widget_);
  auto* multitask_menu_window = multitask_menu_widget_->GetNativeWindow();
  // TODO(sophiewen): Consider adding transient child instead.
  multitask_menu_window->parent()->StackChildAbove(multitask_menu_window,
                                                   window_);

  // Start with the widget offscreen.
  const gfx::Size widget_size =
      multitask_menu_widget_->GetContentsView()->GetPreferredSize();
  const gfx::Rect start_bounds(
      window_->bounds().CenterPoint().x() - widget_size.width() / 2,
      -widget_size.height(), widget_size.width(), widget_size.height());
  multitask_menu_widget_->SetBounds(start_bounds);
  multitask_menu_widget_->Show();
  multitask_menu_widget_->SetOpacity(0.f);

  auto* widget_layer = multitask_menu_widget_->GetLayer();
  const gfx::Rect end_bounds(
      gfx::Point(start_bounds.x(), kMultitaskMenuVerticalPadding), widget_size);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kPositionAnimationDurationMs)
      .SetBounds(widget_layer, end_bounds, gfx::Tween::ACCEL_20_DECEL_100)
      .At(base::Seconds(0))
      .SetDuration(kOpacityAnimationDurationMs)
      .SetOpacity(widget_layer, 1.f, gfx::Tween::LINEAR);
}

void TabletModeMultitaskMenu::AnimateClose() {
  // TODO(crbug.com/1370728): Test animation in portrait mode on secondary
  // window.
  DCHECK(multitask_menu_widget_);
  const gfx::Size widget_size =
      multitask_menu_widget_->GetContentsView()->GetPreferredSize();
  const gfx::Rect end_bounds(
      multitask_menu_widget_->GetWindowBoundsInScreen().x(),
      -widget_size.height() - kMultitaskMenuVerticalPadding,
      widget_size.width(), widget_size.height());
  auto* widget_layer = multitask_menu_widget_->GetLayer();
  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&TabletModeMultitaskMenu::Reset,
                              weak_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kPositionAnimationDurationMs)
      .SetBounds(widget_layer, end_bounds, gfx::Tween::ACCEL_20_DECEL_100)
      .At(base::Seconds(0))
      .SetDuration(kOpacityAnimationDurationMs)
      .SetOpacity(widget_layer, 0.f, gfx::Tween::LINEAR);
}

void TabletModeMultitaskMenu::Reset() {
  event_handler_->ResetMultitaskMenu();
}

void TabletModeMultitaskMenu::OnWindowDestroying(aura::Window* window) {
  DCHECK(observed_window_.IsObservingSource(window));

  observed_window_.Reset();
  window_ = nullptr;

  // Destroys `this`.
  event_handler_->ResetMultitaskMenu();
}

void TabletModeMultitaskMenu::OnWidgetActivationChanged(views::Widget* widget,
                                                        bool active) {
  // `widget` gets deactivated when the window state changes.
  DCHECK(widget_observation_.IsObservingSource(widget));
  if (!active)
    event_handler_->ResetMultitaskMenu();
}

void TabletModeMultitaskMenu::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // The destruction of `multitask_menu_widget_` causes an activation change
  // which can send out a work area change.
  if (!multitask_menu_widget_)
    return;

  // Ignore changes to displays that aren't showing the menu.
  if (display.id() !=
      display::Screen::GetScreen()
          ->GetDisplayNearestView(multitask_menu_widget_->GetNativeWindow())
          .id()) {
    return;
  }
  // TODO(shidi): Will do the rotate transition on a separate cl. Close the
  // menu at rotation for now.
  if (changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)
    event_handler_->ResetMultitaskMenu();
}

chromeos::MultitaskMenuView*
TabletModeMultitaskMenu::GetMultitaskMenuViewForTesting() {
  return static_cast<TabletModeMultitaskMenuView*>(
             multitask_menu_widget_->GetContentsView())
      ->multitask_menu_view_for_testing();  // IN-TEST
}

}  // namespace ash
