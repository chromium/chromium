// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"

#include <algorithm>
#include <bit>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_cue_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_controller.h"
#include "ash/wm/window_state.h"
#include "base/debug/crash_logging.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "chromeos/ui/frame/multitask_menu/split_button_view.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/screen.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// The vertical position of the multitask menu, in the window (and widget)
// coordinates.
constexpr int kVerticalPosition = 8;

// The outset around the menu needed to show the menu shadow.
constexpr int kShadowOutset = 12;

// Menu layout values.
constexpr int kBetweenButtonSpacing = 12;
constexpr int kCornerRadius = 8;
constexpr gfx::Insets kInsideBorderInsets(16);

// Menu animation values.
constexpr base::TimeDelta kPositionAnimationDurationMs =
    base::Milliseconds(250);
constexpr base::TimeDelta kOpacityAnimationDurationMs = base::Milliseconds(150);

}  // namespace

// The contents view of the multitask menu.
class TabletModeMultitaskMenuView : public views::View {
  METADATA_HEADER(TabletModeMultitaskMenuView, views::View)

 public:
  TabletModeMultitaskMenuView(aura::Window* window,
                              base::RepeatingClosure close_callback,
                              base::RepeatingClosure dismiss_callback) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshShieldAndBaseOpaque, kCornerRadius));
    SetBorder(std::make_unique<views::HighlightBorder>(
        kCornerRadius, views::HighlightBorder::Type::kHighlightBorderOnShadow));

    SetUseDefaultFillLayout(true);

    // Since this menu is only shown for maximizable windows, it can be
    // fullscreened.
    auto* window_state = WindowState::Get(window);
    CHECK(window_state);
    CHECK(window_state->CanMaximize());
    uint8_t buttons = chromeos::MultitaskMenuView::kFullscreen;

    auto* split_view_controller = SplitViewController::Get(window);
    if (split_view_controller->CanSnapWindow(window,
                                             chromeos::kDefaultSnapRatio)) {
      buttons |= chromeos::MultitaskMenuView::kHalfSplit;
    }

    if (split_view_controller->CanSnapWindow(window,
                                             chromeos::kTwoThirdSnapRatio)) {
      // If `min_length` <= 2/3, it can fit into 2/3 split, so ensure that
      // we show the full partial button.
      buttons |= chromeos::MultitaskMenuView::kPartialSplit;
    }

    if (chromeos::wm::CanFloatWindow(window)) {
      buttons |= chromeos::MultitaskMenuView::kFloat;
    }

    // TODO(sophiewen): Ensure that there is always 2 buttons or more if this
    // view is created.
    DCHECK_GE(std::bitset<4 + 1>(buttons).count(), 1u);

    menu_view_base_ =
        AddChildView(std::make_unique<chromeos::MultitaskMenuView>(
            window, std::move(close_callback), std::move(dismiss_callback),
            buttons, /*anchor_view=*/nullptr));

    if (menu_view_base_->partial_button() &&
        !split_view_controller->CanSnapWindow(window,
                                              chromeos::kOneThirdSnapRatio)) {
      // Disable the 1/3 option in the partial button if it can't be snapped
      // 1/3. Note that `GetRightBottomButton` must be disabled after
      // `partial_button` is initialized to update the color in
      // `SplitButton::OnPaintBackground()`.
      menu_view_base_->partial_button()->GetRightBottomButton()->SetEnabled(
          false);
    }

    auto* layout =
        menu_view_base_->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, kInsideBorderInsets,
            kBetweenButtonSpacing));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    shadow_ = SystemShadow::CreateShadowOnNinePatchLayer(
        SystemShadow::Type::kElevation12,
        SystemShadow::LayerRecreatedCallback());
    shadow_->SetRoundedCornerRadius(kCornerRadius);
    layer()->Add(shadow_->GetLayer());
  }

  TabletModeMultitaskMenuView(const TabletModeMultitaskMenuView&) = delete;
  TabletModeMultitaskMenuView& operator=(const TabletModeMultitaskMenuView&) =
      delete;
  ~TabletModeMultitaskMenuView() override = default;

  chromeos::MultitaskMenuView* menu_view_base() { return menu_view_base_; }

  SystemShadow* shadow() { return shadow_.get(); }

 private:
  raw_ptr<chromeos::MultitaskMenuView> menu_view_base_ = nullptr;

  std::unique_ptr<SystemShadow> shadow_;
};

BEGIN_METADATA(TabletModeMultitaskMenuView)
END_METADATA

TabletModeMultitaskMenu::TabletModeMultitaskMenu(
    TabletModeMultitaskMenuController* controller,
    aura::Window* window)
    : controller_(controller) {
  CHECK(window);

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  // Use an activatable container that's guaranteed to be on top of the desk
  // containers, otherwise the menu may get stacked under the window on
  // deactivation.
  params.parent = window->GetRootWindow()->GetChildById(
      kShellWindowId_AlwaysOnTopContainer);
  params.name = "TabletModeMultitaskMenuWidget";

  widget_->Init(std::move(params));
  widget_->SetVisibilityChangedAnimationsEnabled(false);
  widget_->widget_delegate()->SetEnableArrowKeyTraversal(true);

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
          window,
          base::BindRepeating(&TabletModeMultitaskMenu::AnimateFadeOut,
                              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&TabletModeMultitaskMenu::Animate,
                              weak_factory_.GetWeakPtr(),
                              /*show=*/false)));

  // Set the widget on the top center of the window.
  const gfx::Size menu_size(menu_view_->GetPreferredSize());
  const gfx::Rect window_bounds(window->GetBoundsInScreen());

  // The invisible widget needs to be big enough to include both the menu and
  // shadow otherwise it would mask parts out. Explicitly set the widget size
  // since `window` may be narrower than the menu and clamp to its bounds.
  const int widget_width = menu_size.width() + kShadowOutset * 2;
  gfx::Rect widget_bounds = gfx::Rect(
      window_bounds.CenterPoint().x() - widget_width / 2, window_bounds.y(),
      widget_width, menu_size.height() + kShadowOutset * 2);
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window);
  if (!work_area.Contains(widget_bounds)) {
    widget_bounds.AdjustToFit(work_area);
  }

  widget_->SetBounds(widget_bounds);
  widget_->Show();

  // Set the menu bounds and apply a transform offscreen.
  menu_view_->SetBounds(kShadowOutset, kVerticalPosition, menu_size.width(),
                        menu_size.height());
  const gfx::Transform initial_transform = gfx::Transform::MakeTranslation(
      0, -menu_size.height() - kVerticalPosition);
  menu_view_->layer()->SetTransform(initial_transform);
  menu_view_->shadow()->SetContentBounds(gfx::Rect(menu_size));
  menu_view_->shadow()->ObserveColorProviderSource(widget_.get());

  // Showing the widget can change native focus (which would result in an
  // immediate closing of the menu). Only start observing after shown.
  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

TabletModeMultitaskMenu::~TabletModeMultitaskMenu() {
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

void TabletModeMultitaskMenu::Animate(bool show) {
  ui::Layer* view_layer = menu_view_->layer();
  if (view_layer->GetAnimator()->is_animating()) {
    return;
  }

  if (show) {
    RecordMultitaskMenuEntryType(
        chromeos::MultitaskMenuEntryType::kGestureScroll);
  }

  views::AnimationBuilder animation_builder;
  animation_builder
      .OnEnded(show ? base::DoNothing()
                    : base::BindRepeating(&TabletModeMultitaskMenu::Reset,
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
                    gfx::Tween::ACCEL_20_DECEL_100);
  ui::Layer* cue_layer = controller_->multitask_cue_controller()->cue_layer();
  if (cue_layer) {
    animation_builder.GetCurrentSequence().SetTransform(
        cue_layer,
        show ? gfx::Transform::MakeTranslation(
                   0,
                   menu_view_->GetPreferredSize().height() + kVerticalPosition)
             : gfx::Transform(),
        gfx::Tween::ACCEL_20_DECEL_100);
  }
}

void TabletModeMultitaskMenu::AnimateFadeOut() {
  ui::Layer* view_layer = menu_view_->layer();
  ui::LayerAnimator* animator = view_layer->GetAnimator();
  if (animator->IsAnimatingOnePropertyOf(ui::LayerAnimationElement::OPACITY)) {
    // If the layer is already fading out, no need to start another one. This
    // can happen, for example, if buttons are clicked rapidly while fade out
    // has started.
    if (view_layer->GetTargetOpacity() == 0.0f) {
      return;
    }
    // Else if we are currently animating to show, abort and start a new fade
    // out animation.
    animator->AbortAllAnimations();
  }

  views::AnimationBuilder animation_builder;
  animation_builder
      .OnEnded(base::BindRepeating(&TabletModeMultitaskMenu::Reset,
                                   weak_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kOpacityAnimationDurationMs)
      .SetOpacity(view_layer, 0.0f, gfx::Tween::LINEAR);

  ui::Layer* cue_layer = controller_->multitask_cue_controller()->cue_layer();
  if (cue_layer) {
    animation_builder.GetCurrentSequence().SetOpacity(cue_layer, 0.0f);
  }
}

void TabletModeMultitaskMenu::BeginDrag(float initial_y, bool down) {
  if (down) {
    // If we are dragging down, the menu hasn't been created yet, so match the
    // bottom of the menu with `initial_y` and save it as `initial_y_`.
    const float translation_y = initial_y - menu_view_->bounds().bottom();
    initial_y_ = menu_view_->bounds().bottom();
    menu_view_->layer()->SetTransform(
        gfx::Transform::MakeTranslation(0, translation_y));
    if (ui::Layer* cue_layer =
            controller_->multitask_cue_controller()->cue_layer()) {
      cue_layer->SetTransform(gfx::Transform::MakeTranslation(0, initial_y));
    }
  } else {
    // Drag up can start from anywhere in the menu; simply save `initial_y` to
    // update drag relative to it.
    initial_y_ = initial_y;
  }
}

void TabletModeMultitaskMenu::UpdateDrag(float current_y, bool down) {
  // Stop translating the menu if the drag moves out of bounds.
  if (current_y <= 0.f ||
      current_y >=
          kVerticalPosition + menu_view_->GetPreferredSize().height()) {
    return;
  }

  ui::Layer* menu_layer = menu_view_->layer();
  ui::LayerAnimator* animator = menu_layer->GetAnimator();
  if (animator->IsAnimatingOnePropertyOf(
          ui::LayerAnimationElement::TRANSFORM)) {
    // Calling `SetTransform()` with the same target transform can end an
    // ongoing animation and destroy `this`. Abort the animation (not stop
    // which will call `AnimationBuilder::OnEnded()`).
    animator->AbortAllAnimations();
  }

  const float translation_y = current_y - initial_y_;
  menu_layer->SetTransform(gfx::Transform::MakeTranslation(0, translation_y));

  if (auto* cue_layer = controller_->multitask_cue_controller()->cue_layer()) {
    cue_layer->SetTransform(gfx::Transform::MakeTranslation(
        0, menu_view_->GetPreferredSize().height() + kVerticalPosition +
               translation_y));
  }
}

void TabletModeMultitaskMenu::EndDrag() {
  // Calculate the `current_translation_y` relative to `max_translation_y`. Both
  // negative values and relative to the menu position.
  const float current_translation_y =
      menu_view_->layer()->transform().To2dTranslation().y();
  const float max_translation_y =
      -menu_view_->GetPreferredSize().height() - kVerticalPosition;
  const float translated_ratio =
      std::clamp(current_translation_y / max_translation_y, 0.f, 1.f);
  Animate(/*show=*/translated_ratio <= 0.5f);
}

void TabletModeMultitaskMenu::Reset() {
  controller_->ResetMultitaskMenu();
}

void TabletModeMultitaskMenu::OnNativeFocusChanged(
    gfx::NativeView focused_now) {
  ui::Layer* view_layer = menu_view_->layer();
  // Prevent fade out while we are animating to show. This can happen if the
  // drag goes out of bounds while the menu is animating.
  if (view_layer->GetAnimator()->is_animating() &&
      view_layer->GetTargetOpacity() == 1.0f) {
    return;
  }

  if (widget_->GetNativeView() != focused_now) {
    // Destroys `this` at the end of animation.
    AnimateFadeOut();
  }
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
  if (changed_metrics) {
    AnimateFadeOut();
  }
}

chromeos::MultitaskMenuView*
TabletModeMultitaskMenu::GetMultitaskMenuViewForTesting() {
  return menu_view_->menu_view_base();  // IN-TEST
}

}  // namespace ash
