// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_button.h"

#include <math.h>  // std::ceil
#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/check_op.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/controls/button/button_controller.h"

namespace ash {
namespace {

constexpr uint8_t kAssistantVisibleAlpha = 255;    // 100% alpha
constexpr uint8_t kAssistantInvisibleAlpha = 138;  // 54% alpha

// Nudge animation constants

// The offsets that the home button moves up/down from the original home button
// position at each stage of nudge animation.
constexpr int kAnimationBounceUpOffset = 12;
constexpr int kAnimationBounceDownOffset = 3;

constexpr base::TimeDelta kHomeButtonAnimationDuration =
    base::Milliseconds(250);
constexpr base::TimeDelta kRippleAnimationDuration = base::Milliseconds(2000);

}  // namespace

// static
const char HomeButton::kViewClassName[] = "ash/HomeButton";

// HomeButton::ScopedNoClipRect ------------------------------------------------

HomeButton::ScopedNoClipRect::ScopedNoClipRect(
    ShelfNavigationWidget* shelf_navigation_widget)
    : shelf_navigation_widget_(shelf_navigation_widget),
      clip_rect_(shelf_navigation_widget_->GetLayer()->clip_rect()) {
  shelf_navigation_widget_->GetLayer()->SetClipRect(gfx::Rect());
}

HomeButton::ScopedNoClipRect::~ScopedNoClipRect() {
  // The shelf_navigation_widget_ may be destructed before this dtor is
  // called.
  if (shelf_navigation_widget_->GetLayer())
    shelf_navigation_widget_->GetLayer()->SetClipRect(clip_rect_);
}

// HomeButton::ScopedNoClipRect ------------------------------------------------

HomeButton::HomeButton(Shelf* shelf)
    : ShelfControlButton(shelf, this), controller_(this) {
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetHasInkDropActionOnClick(false);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  layer()->SetName("shelf/Homebutton");
}

HomeButton::~HomeButton() = default;

void HomeButton::OnGestureEvent(ui::GestureEvent* event) {
  if (!controller_.MaybeHandleGestureEvent(event))
    Button::OnGestureEvent(event);
}

std::u16string HomeButton::GetTooltipText(const gfx::Point& p) const {
  // Don't show a tooltip if we're already showing the app list.
  return IsShowingAppList() ? std::u16string() : GetAccessibleName();
}

const char* HomeButton::GetClassName() const {
  return kViewClassName;
}

void HomeButton::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {
  DCHECK_EQ(button, this);
  // Focus out if:
  // *   The currently focused view is already this button, which implies that
  //     this is the only button in this widget.
  // *   Going in reverse when the shelf has a back button, which implies that
  //     the widget is trying to loop back from the back button.
  if (GetFocusManager()->GetFocusedView() == this ||
      (reverse && shelf()->navigation_widget()->GetBackButton())) {
    shelf()->shelf_focus_cycler()->FocusOut(reverse,
                                            SourceView::kShelfNavigationView);
  }
}

void HomeButton::ButtonPressed(views::Button* sender,
                               const ui::Event& event,
                               views::InkDrop* ink_drop) {
  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    base::RecordAction(
        base::UserMetricsAction("AppList_HomeButtonPressedTablet"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("AppList_HomeButtonPressedClamshell"));
  }

  const AppListShowSource show_source =
      event.IsShiftDown() ? kShelfButtonFullscreen : kShelfButton;
  Shell::Get()->app_list_controller()->ToggleAppList(
      GetDisplayId(), show_source, event.time_stamp());
}

void HomeButton::OnAssistantAvailabilityChanged() {
  SchedulePaint();
}

bool HomeButton::IsShowingAppList() const {
  return controller_.is_showing_app_list();
}

void HomeButton::HandleLocaleChange() {
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE));
  TooltipTextChanged();
  // Reset the bounds rect so the child layer bounds get updated on next shelf
  // layout if the RTL changed.
  SetBoundsRect(gfx::Rect());
}

int64_t HomeButton::GetDisplayId() const {
  aura::Window* window = GetWidget()->GetNativeWindow();
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
}

std::unique_ptr<HomeButton::ScopedNoClipRect>
HomeButton::CreateScopedNoClipRect() {
  return std::make_unique<HomeButton::ScopedNoClipRect>(
      shelf()->navigation_widget());
}

void HomeButton::StartNudgeAnimation() {
  // Create the ripple layer and its delegate for the nudge animation.
  nudge_ripple_layer_ = std::make_unique<ui::Layer>();
  float ripple_diameter = layer()->size().width();
  ripple_layer_delegate_ = std::make_unique<views::CircleLayerDelegate>(
      AshColorProvider::Get()->GetInkDropBaseColorAndOpacity().first,
      /*radius=*/ripple_diameter / 2);

  // The bounds are set with respect to |shelf_container_layer| stated below.
  nudge_ripple_layer_->SetBounds(
      gfx::Rect(layer()->parent()->bounds().x() + bounds().x(),
                layer()->parent()->bounds().y() + bounds().y(), ripple_diameter,
                ripple_diameter));
  nudge_ripple_layer_->set_delegate(ripple_layer_delegate_.get());
  nudge_ripple_layer_->SetMasksToBounds(true);
  nudge_ripple_layer_->SetFillsBoundsOpaquely(false);

  // The position of the ripple layer is independent to the home button and its
  // parent shelf navigation widget. Therefore the ripple layer is added to the
  // shelf container layer, which is the parent layer of the shelf navigation
  // widget.
  ui::Layer* shelf_container_layer = GetWidget()->GetLayer()->parent();
  shelf_container_layer->Add(nudge_ripple_layer_.get());
  shelf_container_layer->StackBelow(nudge_ripple_layer_.get(),
                                    layer()->parent());

  // Home button movement settings. Note that the navigation widget layer
  // contains the non-opaque part of the home button and is also animated along
  // with the home button.
  ui::Layer* widget_layer = GetWidget()->GetLayer();

  gfx::PointF bounce_up_point = shelf()->SelectValueForShelfAlignment(
      gfx::PointF(0, -kAnimationBounceUpOffset),
      gfx::PointF(kAnimationBounceUpOffset, 0),
      gfx::PointF(-kAnimationBounceUpOffset, 0));
  gfx::PointF bounce_down_point = shelf()->SelectValueForShelfAlignment(
      gfx::PointF(0, kAnimationBounceDownOffset),
      gfx::PointF(-kAnimationBounceDownOffset, 0),
      gfx::PointF(kAnimationBounceDownOffset, 0));

  gfx::Transform move_up;
  move_up.Translate(bounce_up_point.x(), bounce_up_point.y());
  gfx::Transform move_down;
  move_down.Translate(bounce_down_point.x(), bounce_down_point.y());

  gfx::Transform initial_disc_scale;
  initial_disc_scale.Scale(0.1f, 0.1f);
  gfx::Transform initial_state =
      gfx::TransformAboutPivot(GetCenterPoint(), initial_disc_scale);

  gfx::Transform final_disc_scale;
  final_disc_scale.Scale(3.0f, 3.0f);
  gfx::Transform scale_about_pivot =
      gfx::TransformAboutPivot(GetCenterPoint(), final_disc_scale);

  // Remove clip_rect from the home button and its ancestors as the animation
  // goes beyond its size. The object is deleted once the animation ends.
  scoped_no_clip_rect_ = CreateScopedNoClipRect();

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnStarted(base::BindOnce(&HomeButton::OnNudgeAnimationStarted,
                                weak_ptr_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(&HomeButton::OnNudgeAnimationEnded,
                              weak_ptr_factory_.GetWeakPtr()))
      .Once()
      // Set up the animation of the ripple_layer
      .SetDuration(base::TimeDelta())
      .SetTransform(nudge_ripple_layer_.get(), initial_state)
      .SetOpacity(nudge_ripple_layer_.get(), 0.5f)
      .Then()
      .SetDuration(kRippleAnimationDuration)
      .SetTransform(nudge_ripple_layer_.get(), scale_about_pivot,
                    gfx::Tween::ACCEL_0_40_DECEL_100)
      .SetOpacity(nudge_ripple_layer_.get(), 0.0f,
                  gfx::Tween::ACCEL_0_80_DECEL_80)
      // Set up the animation of the widget_layer
      .At(base::Seconds(0))
      .SetDuration(kHomeButtonAnimationDuration)
      .SetTransform(widget_layer, move_up, gfx::Tween::FAST_OUT_SLOW_IN_3)
      .Then()
      .SetDuration(kHomeButtonAnimationDuration)
      .SetTransform(widget_layer, move_down, gfx::Tween::ACCEL_80_DECEL_20)
      .Then()
      .SetDuration(kHomeButtonAnimationDuration)
      .SetTransform(widget_layer, gfx::Transform(),
                    gfx::Tween::FAST_OUT_SLOW_IN_3);
}

void HomeButton::AddNudgeAnimationObserverForTest(
    NudgeAnimationObserver* observer) {
  observers_.AddObserver(observer);
}
void HomeButton::RemoveNudgeAnimationObserverForTest(
    NudgeAnimationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void HomeButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::PointF circle_center(GetCenterPoint());

  // Paint a white ring as the foreground for the app list circle. The ceil/dsf
  // math assures that the ring draws sharply and is centered at all scale
  // factors.
  float ring_outer_radius_dp = 7.f;
  float ring_thickness_dp = 1.5f;
  if (controller_.IsAssistantAvailable()) {
    ring_outer_radius_dp = 8.f;
    ring_thickness_dp = 1.f;
  }
  {
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float dsf = canvas->UndoDeviceScaleFactor();
    circle_center.Scale(dsf);
    cc::PaintFlags fg_flags;
    fg_flags.setAntiAlias(true);
    fg_flags.setStyle(cc::PaintFlags::kStroke_Style);
    fg_flags.setColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor));

    if (controller_.IsAssistantAvailable()) {
      // active: 100% alpha, inactive: 54% alpha
      fg_flags.setAlpha(controller_.IsAssistantVisible()
                            ? kAssistantVisibleAlpha
                            : kAssistantInvisibleAlpha);
    }

    const float thickness = std::ceil(ring_thickness_dp * dsf);
    const float radius = std::ceil(ring_outer_radius_dp * dsf) - thickness / 2;
    fg_flags.setStrokeWidth(thickness);
    // Make sure the center of the circle lands on pixel centers.
    canvas->DrawCircle(circle_center, radius, fg_flags);

    if (controller_.IsAssistantAvailable()) {
      fg_flags.setAlpha(255);
      const float kCircleRadiusDp = 5.f;
      fg_flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->DrawCircle(circle_center, std::ceil(kCircleRadiusDp * dsf),
                         fg_flags);
    }
  }
}

void HomeButton::OnThemeChanged() {
  ShelfControlButton::OnThemeChanged();
  if (ripple_layer_delegate_) {
    ripple_layer_delegate_->set_color(
        AshColorProvider::Get()->GetInkDropBaseColorAndOpacity().first);
  }
  SchedulePaint();
}

bool HomeButton::DoesIntersectRect(const views::View* target,
                                   const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  gfx::Rect button_bounds = target->GetLocalBounds();
  // Increase clickable area for the button to account for clicks around the
  // spacing. This will not intercept events outside of the parent widget.
  button_bounds.Inset(
      gfx::Insets::VH(-ShelfConfig::Get()->control_button_edge_spacing(
                          !shelf()->IsHorizontalAlignment()),
                      -ShelfConfig::Get()->control_button_edge_spacing(
                          shelf()->IsHorizontalAlignment())));
  return button_bounds.Intersects(rect);
}

void HomeButton::OnNudgeAnimationStarted() {
  for (auto& observer : observers_)
    observer.NudgeAnimationStarted(this);
}

void HomeButton::OnNudgeAnimationEnded() {
  // Delete the ripple layer and its delegate after the launcher nudge animation
  // is completed.
  nudge_ripple_layer_.reset();
  ripple_layer_delegate_.reset();

  // Reset the clip rect after the animation is completed.
  scoped_no_clip_rect_.reset();

  for (auto& observer : observers_)
    observer.NudgeAnimationEnded(this);
}

}  // namespace ash
