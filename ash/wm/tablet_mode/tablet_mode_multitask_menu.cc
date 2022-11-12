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

// The vertical position of the multitask menu, in the window (and widget)
// coordinates.
constexpr int kVerticalPosition = 8;

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
                              base::RepeatingClosure callback) {
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
            window, std::move(callback), buttons));

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
    aura::Window* window)
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
  auto* multitask_menu_window = widget_->GetNativeWindow();
  // TODO(sophiewen): Consider adding transient child instead.
  multitask_menu_window->parent()->StackChildAbove(multitask_menu_window,
                                                   window_);

  // Clip the widget's root view so that the menu appears to be sliding out from
  // the top, even if the window above it is stacked below it, which is the case
  // when we are bottom snapped in portrait mode, and the wallpaper is visible
  // in the top snapped section. `SetMasksToBounds` is recommended over
  // `SetClipRect`, which is relative to the layer and would clip within its own
  // bounds.
  views::View* root_view = widget_->GetRootView();
  root_view->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  root_view->layer()->SetMasksToBounds(true);

  menu_view_ =
      widget_->SetContentsView(std::make_unique<TabletModeMultitaskMenuView>(
          window_, base::BindRepeating(&TabletModeMultitaskMenu::Reset,
                                       weak_factory_.GetWeakPtr())));

  const gfx::Size menu_size = menu_view_->GetPreferredSize();
  const gfx::Size widget_size(menu_size.width(),
                              menu_size.height() + kVerticalPosition);
  const gfx::Point widget_origin(
      window_->bounds().CenterPoint().x() - widget_size.width() / 2,
      window_->bounds().y());
  widget_->SetBounds(gfx::Rect(widget_origin, widget_size));
  widget_->Show();

  // Set the menu bounds and apply a transform offscreen.
  const gfx::Point menu_origin(0, kVerticalPosition);
  menu_view_->SetBoundsRect(gfx::Rect(menu_origin, menu_size));
  menu_view_->layer()->SetTransform(gfx::Transform::MakeTranslation(
      0, -menu_view_->GetPreferredSize().height() - kVerticalPosition));

  // TODO(sophiewen): Add shadows on `menu_view_`.

  widget_observation_.Observe(widget_.get());
}

TabletModeMultitaskMenu::~TabletModeMultitaskMenu() = default;

void TabletModeMultitaskMenu::Animate(bool show) {
  ui::Layer* view_layer = menu_view_->layer();
  auto* animator = view_layer->GetAnimator();
  if (animator->is_animating())
    return;
  views::AnimationBuilder()
      .OnEnded(show ? base::DoNothing()
                    : base::BindOnce(&TabletModeMultitaskMenu::Reset,
                                     weak_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kPositionAnimationDurationMs)
      .SetTransform(view_layer,
                    show ? gfx::Transform()
                         : gfx::Transform::MakeTranslation(
                               0, -menu_view_->GetPreferredSize().height() -
                                      kVerticalPosition),
                    gfx::Tween::ACCEL_20_DECEL_100)
      .SetOpacity(view_layer, show ? 1.f : 0.f, gfx::Tween::LINEAR);
}

void TabletModeMultitaskMenu::BeginDrag(float initial_y) {
  // Try to match the bottom of the menu with the drag location.
  // TODO(sophiewen): The drag location may not always be at the bottom of the
  // menu.
  float delta_y = initial_y - menu_view_->bounds().bottom();
  menu_view_->layer()->SetTransform(
      gfx::Transform::MakeTranslation(0, delta_y));
}

void TabletModeMultitaskMenu::UpdateDrag(float current_y) {
  float transform_y = current_y - menu_view_->bounds().bottom();
  menu_view_->layer()->SetTransform(
      gfx::Transform::MakeTranslation(0, transform_y));
}

void TabletModeMultitaskMenu::EndDrag() {
  // Calculate the `current_translation_y` relative to `max_translation_y`. Both
  // negative values and relative to the menu position.
  float current_translation_y =
      menu_view_->layer()->transform().To2dTranslation().y();
  float max_translation_y =
      -menu_view_->GetPreferredSize().height() - kVerticalPosition;
  const float translated_ratio =
      base::clamp(current_translation_y / max_translation_y, 0.f, 1.f);

  Animate(/*show=*/translated_ratio < 0.5f);
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
