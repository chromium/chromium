// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"

#include "ash/style/ash_color_id.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"
#include "ash/wm/window_state.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/screen.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// The vertical position of the multitask menu on the window.
constexpr int kVerticalPosition = 8;

// Outset around the multitask menu widget to show shadows and extend touch hit
// bounds. Vertical outset should be at least as big as `kVerticalPosition`
// to show animations starting from the top of the window.
constexpr gfx::Outsets kWidgetOutsets = gfx::Outsets::VH(kVerticalPosition, 5);

constexpr int kBetweenButtonSpacing = 12;
constexpr int kCornerRadius = 8;
constexpr gfx::Insets kInsideBorderInsets(16);

// The duration of the menu position animation.
constexpr base::TimeDelta kPositionAnimationDurationMs =
    base::Milliseconds(250);

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

    menu_view_for_testing_ =
        AddChildView(std::make_unique<chromeos::MultitaskMenuView>(
            window, hide_menu, buttons));

    auto* layout = menu_view_for_testing_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, kInsideBorderInsets,
            kBetweenButtonSpacing));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }

  TabletModeMultitaskMenuView(const TabletModeMultitaskMenuView&) = delete;
  TabletModeMultitaskMenuView& operator=(const TabletModeMultitaskMenuView&) =
      delete;
  ~TabletModeMultitaskMenuView() override = default;

  chromeos::MultitaskMenuView* menu_view_for_testing() {
    return menu_view_for_testing_;
  }

 private:
  raw_ptr<chromeos::MultitaskMenuView> menu_view_for_testing_ = nullptr;
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

  widget_->Init(std::move(params));
  widget_->SetVisibilityChangedAnimationsEnabled(false);

  // Clip the widget's root view so that the menu appears to be sliding out from
  // the top, even if the window above it is stacked below it, which is the case
  // when we are bottom snapped in portrait mode, and the wallpaper is visible
  // in the top snapped section. `SetMasksToBounds` is recommended over
  // `SetClipRect`, which is relative to the layer and would clip within its own
  // bounds.
  views::View* root_view = widget_->GetRootView();
  root_view->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  root_view->layer()->SetMasksToBounds(true);

  menu_view_ = widget_->SetContentsView(
      std::make_unique<TabletModeMultitaskMenuView>(window_, callback));
  menu_view_->SizeToPreferredSize();

  // TODO(sophiewen): Add shadows on `menu_view_`.

  AnimateShow();

  widget_observation_.Observe(widget_.get());
}

TabletModeMultitaskMenu::~TabletModeMultitaskMenu() = default;

void TabletModeMultitaskMenu::AnimateShow() {
  DCHECK(widget_);
  auto* multitask_menu_window = widget_->GetNativeWindow();
  // TODO(sophiewen): Consider adding transient child instead.
  multitask_menu_window->parent()->StackChildAbove(multitask_menu_window,
                                                   window_);
  widget_->Show();

  // Position the widget on the top center of the window.
  const gfx::Size widget_size = widget_->GetContentsView()->GetPreferredSize();
  const gfx::Point widget_origin(
      window_->bounds().CenterPoint().x() - widget_size.width() / 2,
      window_->bounds().y() + kVerticalPosition);
  widget_->SetBounds(gfx::Rect(widget_origin, widget_size));

  const gfx::Transform transform = gfx::Transform::MakeTranslation(
      0, -widget_size.height() - kVerticalPosition);

  ui::Layer* view_layer = menu_view_->layer();
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetTransform(view_layer, transform)
      .SetOpacity(view_layer, 0.f)
      .Then()
      .SetDuration(kPositionAnimationDurationMs)
      .SetTransform(view_layer, gfx::Transform(),
                    gfx::Tween::ACCEL_20_DECEL_100)
      .SetOpacity(view_layer, 1.f, gfx::Tween::LINEAR);
}

void TabletModeMultitaskMenu::AnimateClose() {
  DCHECK(widget_);

  // Since the widget gets destroyed after the animation, its bounds don't need
  // to be set.
  const gfx::Size pref_size = menu_view_->GetPreferredSize();
  const gfx::Transform transform = gfx::Transform::MakeTranslation(
      0, -pref_size.height() - kVerticalPosition - kWidgetOutsets.height());

  ui::Layer* view_layer = menu_view_->layer();
  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&TabletModeMultitaskMenu::Reset,
                              weak_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kPositionAnimationDurationMs)
      .SetTransform(view_layer, transform, gfx::Tween::ACCEL_20_DECEL_100)
      .SetOpacity(view_layer, 0.f, gfx::Tween::LINEAR);
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
  // The destruction of `widget_` causes an activation change which can
  // send out a work area change.
  if (!widget_)
    return;

  // Ignore changes to displays that aren't showing the menu.
  if (display.id() != display::Screen::GetScreen()
                          ->GetDisplayNearestView(widget_->GetNativeWindow())
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
  return static_cast<TabletModeMultitaskMenuView*>(menu_view_)
      ->menu_view_for_testing();  // IN-TEST
}

}  // namespace ash
