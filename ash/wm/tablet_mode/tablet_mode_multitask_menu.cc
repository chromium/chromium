// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"
#include "ash/wm/window_state.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "chromeos/ui/frame/multitask_menu/split_button_view.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/aura/window_delegate.h"
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
 public:
  METADATA_HEADER(TabletModeMultitaskMenuView);

  TabletModeMultitaskMenuView(aura::Window* window,
                              base::RepeatingClosure callback) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshShieldAndBaseOpaque, kCornerRadius));
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
    }

    // TODO(sophiewen): Refactor from SplitViewController.
    DCHECK(window->delegate());
    const bool horizontal = chromeos::IsDisplayLayoutHorizontal(
        display::Screen::GetScreen()->GetDisplayNearestWindow(window));
    const gfx::Size min_size = window->delegate()->GetMinimumSize();
    const int min_length = horizontal ? min_size.width() : min_size.height();
    const gfx::Rect work_area = display::Screen::GetScreen()
                                    ->GetDisplayNearestWindow(window)
                                    .work_area();
    const int work_area_length =
        horizontal ? work_area.width() : work_area.height();
    if (min_length <= work_area_length * chromeos::kTwoThirdSnapRatio) {
      // If `min_length` <= 2/3, it can fit into 2/3 split, so ensure that
      // we show the full partial button.
      buttons |= chromeos::MultitaskMenuView::kPartialSplit;
    }

    if (chromeos::wm::CanFloatWindow(window))
      buttons |= chromeos::MultitaskMenuView::kFloat;

    menu_view_base_ =
        AddChildView(std::make_unique<chromeos::MultitaskMenuView>(
            window, std::move(callback), buttons));

    if (menu_view_base_->partial_button() &&
        min_length > work_area_length * chromeos::kOneThirdSnapRatio) {
      // If `min_length` > 1/3, it can't fit into 1/3 split, so disable
      // the 1/3 option in the partial button. Note that `GetRightBottomButton`
      // must be disabled after `partial_button` is initialized to update the
      // color in `SplitButton::OnPaintBackground()`.
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
        SystemShadow::Type::kElevation12);
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
  params.parent =
      window->GetRootWindow()->GetChildById(kShellWindowId_FloatContainer);
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

  menu_view_ =
      widget_->SetContentsView(std::make_unique<TabletModeMultitaskMenuView>(
          window_, base::BindRepeating(&TabletModeMultitaskMenu::Reset,
                                       weak_factory_.GetWeakPtr())));

  // Set the widget on the top center of the window.
  const gfx::Size menu_size(menu_view_->GetPreferredSize());
  const gfx::Rect window_bounds(window_->GetBoundsInScreen());

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

  // Showing the widget can change native focus (which would result in an
  // immediate closing of the menu). Only start observing after shown.
  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

TabletModeMultitaskMenu::~TabletModeMultitaskMenu() {
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

void TabletModeMultitaskMenu::Animate(bool show) {
  ui::Layer* view_layer = menu_view_->layer();
  if (view_layer->GetAnimator()->is_animating())
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
                    gfx::Tween::ACCEL_20_DECEL_100);
}

void TabletModeMultitaskMenu::AnimateFadeOut() {
  ui::Layer* view_layer = menu_view_->layer();
  if (view_layer->GetAnimator()->is_animating())
    return;
  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&TabletModeMultitaskMenu::Reset,
                              weak_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kOpacityAnimationDurationMs)
      .SetOpacity(view_layer, 0.0f, gfx::Tween::LINEAR);
}

void TabletModeMultitaskMenu::BeginDrag(float initial_y, bool down) {
  if (down) {
    // If we are dragging down, the menu hasn't been created yet, so match the
    // bottom of the menu with `initial_y` and save it as `initial_y_`.
    const float translation_y = initial_y - menu_view_->bounds().bottom();
    initial_y_ = menu_view_->bounds().bottom();
    menu_view_->layer()->SetTransform(
        gfx::Transform::MakeTranslation(0, translation_y));
  } else {
    // Drag up can start from anywhere in the menu; simply save `initial_y` to
    // update drag relative to it.
    initial_y_ = initial_y;
  }
}

void TabletModeMultitaskMenu::UpdateDrag(float current_y, bool down) {
  const float translation_y = current_y - initial_y_;
  // Stop translating the menu if the drag moves out of bounds.
  if (down && translation_y >= 0.f) {
    return;
  }
  menu_view_->layer()->SetTransform(
      gfx::Transform::MakeTranslation(0, translation_y));
}

void TabletModeMultitaskMenu::EndDrag() {
  // Calculate the `current_translation_y` relative to `max_translation_y`. Both
  // negative values and relative to the menu position.
  const float current_translation_y =
      menu_view_->layer()->transform().To2dTranslation().y();
  const float max_translation_y =
      -menu_view_->GetPreferredSize().height() - kVerticalPosition;
  const float translated_ratio =
      base::clamp(current_translation_y / max_translation_y, 0.f, 1.f);
  Animate(/*show=*/translated_ratio <= 0.5f);
}

void TabletModeMultitaskMenu::Reset() {
  event_handler_->ResetMultitaskMenu();
}

void TabletModeMultitaskMenu::OnWindowDestroying(aura::Window* window) {
  DCHECK(observed_window_.IsObservingSource(window));

  observed_window_.Reset();
  window_ = nullptr;

  // Destroys `this`.
  Reset();
}

void TabletModeMultitaskMenu::OnNativeFocusChanged(
    gfx::NativeView focused_now) {
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
  return static_cast<TabletModeMultitaskMenuView*>(menu_view_)
      ->menu_view_base();  // IN-TEST
}

}  // namespace ash
