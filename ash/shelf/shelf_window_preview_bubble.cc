// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_window_preview_bubble.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/window_preview_view.h"
#include "base/check_op.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_target.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Returns the bubble arrow position for a preview bubble anchored to the shelf
// tooltip, placing the preview bubble above the tooltip for a horizontal shelf
// and below the tooltip for a vertical shelf.
views::BubbleBorder::Arrow GetArrowForShelf(ShelfBubble* anchor) {
  CHECK(anchor);
  return Shelf::ForWindow(anchor->GetWidget()->GetNativeWindow())
                 ->IsHorizontalAlignment()
             ? views::BubbleBorder::BOTTOM_CENTER
             : views::BubbleBorder::TOP_CENTER;
}

// Calculates the preferred size for the preview bubble of |window| based on its
// aspect ratio and the height and width limits configured in ShelfConfig.
gfx::Size GetPreviewSize(aura::Window* window) {
  CHECK(window);
  float ratio = 1.0f;
  if (!window->bounds().IsEmpty()) {
    ratio = static_cast<float>(window->bounds().width()) /
            window->bounds().height();
  }
  const ShelfConfig* config = ShelfConfig::Get();
  ratio = std::clamp(ratio, config->shelf_preview_min_ratio(),
                     config->shelf_preview_max_ratio());
  const int preview_height = config->shelf_preview_height();
  const int preview_max_width = config->shelf_preview_max_width();
  int width =
      std::min(static_cast<int>(preview_height * ratio), preview_max_width);
  return gfx::Size(width, preview_height);
}

// An observer that watches the detached layer fade-out animation and deletes
// itself and the detached layer tree upon completion or abortion.
class FadeOutAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit FadeOutAnimationObserver(
      std::unique_ptr<ui::LayerTreeOwner> layer_tree)
      : layer_tree_(std::move(layer_tree)) {}

  FadeOutAnimationObserver(const FadeOutAnimationObserver&) = delete;
  FadeOutAnimationObserver& operator=(const FadeOutAnimationObserver&) = delete;

  ~FadeOutAnimationObserver() override = default;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  std::unique_ptr<ui::LayerTreeOwner> layer_tree_;
};

// Duration of the fade-in and fade-out animations for the preview bubble.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(150);

}  // namespace

ShelfWindowPreviewBubble::ShelfWindowPreviewBubble(
    views::MenuItemView* anchor,
    aura::Window* window,
    base::OnceClosure dismiss_callback)
    : ShelfWindowPreviewBubble(anchor,
                               window,
                               GetArrowForMenuItem(anchor),
                               std::move(dismiss_callback)) {}

ShelfWindowPreviewBubble::ShelfWindowPreviewBubble(
    ShelfBubble* anchor,
    aura::Window* window,
    base::OnceClosure dismiss_callback)
    : ShelfWindowPreviewBubble(anchor,
                               window,
                               GetArrowForShelf(anchor),
                               std::move(dismiss_callback)) {}

ShelfWindowPreviewBubble::ShelfWindowPreviewBubble(
    views::View* anchor,
    aura::Window* window,
    views::BubbleBorder::Arrow arrow,
    base::OnceClosure dismiss_callback)
    : ShelfBubble(anchor,
                  ShelfAlignment::kBottom,
                  /*for_tooltip=*/false,
                  arrow),
      window_(window),
      dismiss_callback_(std::move(dismiss_callback)) {
  CHECK(anchor);
  CHECK(window_);
  CHECK(dismiss_callback_);
  set_close_on_deactivate(false);
  SetCanActivate(false);
  set_accept_events(false);
  set_border_radius(views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(gfx::Insets(0));

  set_available_screen_bounds_callback(base::BindRepeating(
      [](views::View* anchor, const gfx::Rect& rect) {
        if (anchor && anchor->GetWidget() &&
            anchor->GetWidget()->GetNativeWindow()) {
          return display::Screen::Get()
              ->GetDisplayNearestWindow(anchor->GetWidget()->GetNativeWindow())
              .work_area();
        }
        return display::Screen::Get()
            ->GetDisplayNearestPoint(rect.CenterPoint())
            .work_area();
      },
      anchor));

  window_observation_.Observe(window_);
  CreatePreviewView();

  CreateBubble();

  aura::Window* native_window = GetWidget()->GetNativeWindow();
  CollisionDetectionUtils::IgnoreWindowForCollisionDetection(native_window);
  ::wm::SetWindowVisibilityAnimationType(
      native_window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ::wm::SetWindowVisibilityAnimationTransition(native_window,
                                               ::wm::ANIMATE_NONE);

  ui::Layer* layer = GetWidget()->GetLayer();
  layer->SetOpacity(0.0f);
  GetWidget()->Show();
  {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(kAnimationDuration);
    settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer->SetOpacity(1.0f);
  }

  AddPreTargetHandler();
}

ShelfWindowPreviewBubble::~ShelfWindowPreviewBubble() {
  RemovePreTargetHandler();
}

bool ShelfWindowPreviewBubble::ShouldCloseOnPressDown() {
  return true;
}

bool ShelfWindowPreviewBubble::ShouldCloseOnMouseExit() {
  return true;
}

void ShelfWindowPreviewBubble::OnEvent(ui::Event* event) {
  // Handle global `Shell` pre-target events directly without forwarding to
  // `ShelfBubble::OnEvent` (`View::OnTouchEvent` hits `NOTREACHED()`, and
  // `View` mouse/key handlers expect widget-targeted events).
  CHECK_EQ(event->phase(), ui::EP_PRETARGET);

  if (event->IsMouseEvent()) {
    ui::MouseEvent* mouse_event = event->AsMouseEvent();
    if (mouse_event->type() == ui::EventType::kMousePressed &&
        mouse_event->IsOnlyLeftMouseButton()) {
      ProcessPressedEvent(mouse_event);
    }
    return;
  }

  if (event->IsTouchEvent()) {
    ui::TouchEvent* touch_event = event->AsTouchEvent();
    aura::Window* target = static_cast<aura::Window*>(touch_event->target());
    CHECK(target);
    if (touch_event->type() == ui::EventType::kTouchPressed &&
        !gesture_consumer_ &&
        IsPointOverBubble(target->GetScreenLocation(*touch_event))) {
      CHECK(target->provider());
      if (target->provider()->pointer_state().GetPointerCount() == 1) {
        gesture_consumer_ = target->GetWeakPtrAsWindow();
      }
    }
    if (target == gesture_consumer_.get()) {
      // Stop propagation so `ShelfTooltipManager`, `MenuController`, and
      // windows behind the preview do not react to raw touches, while still
      // allowing `GestureRecognizer` to generate gestures.
      touch_event->StopPropagation();
      touch_event->ForceProcessGesture();
    }
    return;
  }

  if (event->IsGestureEvent()) {
    ui::GestureEvent* gesture_event = event->AsGestureEvent();
    CHECK(gesture_event->target());
    if (gesture_event->target() != gesture_consumer_.get()) {
      return;
    }
    gesture_event->SetHandled();
    gesture_event->StopPropagation();
    if (gesture_event->type() == ui::EventType::kGestureTap) {
      // `ProcessPressedEvent` may synchronously destroy `this`.
      ProcessPressedEvent(gesture_event);
      return;
    }
    if (gesture_event->type() == ui::EventType::kGestureEnd &&
        gesture_event->details().touch_points() == 1) {
      gesture_consumer_.reset();
    }
  }
}

void ShelfWindowPreviewBubble::OnWindowDestroying(aura::Window* window) {
  if (window_ == window) {
    RemovePreTargetHandler();
    window_observation_.Reset();
    window_ = nullptr;
    RemovePreviewView();
    if (GetWidget()) {
      GetWidget()->CloseNow();
    }
  }
}

void ShelfWindowPreviewBubble::UpdateAnchorAndWindow(
    views::MenuItemView* anchor,
    aura::Window* window) {
  CHECK(anchor);
  CHECK(window);

  if (window_ == window && GetAnchorView() == anchor) {
    return;
  }

  window_observation_.Reset();
  window_ = window;
  window_observation_.Observe(window_);

  SetArrow(GetArrowForMenuItem(anchor));
  SetAnchorView(anchor);

  RemovePreviewView();
  CreatePreviewView();

  SizeToContents();
}

void ShelfWindowPreviewBubble::FadeOutAndClose() {
  RemovePreTargetHandler();
  views::Widget* widget = GetWidget();
  CHECK(widget);

  std::unique_ptr<ui::LayerTreeOwner> layer_tree =
      ::wm::RecreateLayers(widget->GetNativeWindow());
  ui::Layer* animating_layer = layer_tree->root();

  widget->CloseNow();

  auto* observer = new FadeOutAnimationObserver(std::move(layer_tree));
  ui::ScopedLayerAnimationSettings settings(animating_layer->GetAnimator());
  settings.AddObserver(observer);
  settings.SetTransitionDuration(kAnimationDuration);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animating_layer->SetOpacity(0.0f);
}

bool ShelfWindowPreviewBubble::IsPointInPreviewRegion(
    const gfx::Point& screen_point) const {
  if (!GetWidget()) {
    return false;
  }
  // Check `(preview or anchor)` and `(anchor or shelf item)` separately rather
  // than a single 3-way union so a wide preview does not keep the tooltip open
  // over neighboring empty shelf space.
  gfx::Rect preview_to_anchor = GetWidget()->GetWindowBoundsInScreen();
  const views::View* anchor = GetAnchorView();
  if (!anchor) {
    return preview_to_anchor.Contains(screen_point);
  }
  const gfx::Rect anchor_bounds =
      anchor->GetWidget() ? anchor->GetWidget()->GetWindowBoundsInScreen()
                          : anchor->GetBoundsInScreen();
  preview_to_anchor.Union(anchor_bounds);
  if (preview_to_anchor.Contains(screen_point)) {
    return true;
  }
  if (const auto* anchor_bubble = views::AsViewClass<ShelfBubble>(anchor)) {
    if (const views::View* root_anchor = anchor_bubble->GetAnchorView()) {
      gfx::Rect anchor_to_root = anchor_bounds;
      anchor_to_root.Union(root_anchor->GetBoundsInScreen());
      return anchor_to_root.Contains(screen_point);
    }
  }
  return false;
}

void ShelfWindowPreviewBubble::CreatePreviewView() {
  CHECK(!preview_view_);
  CHECK(window_);
  auto preview_view =
      std::make_unique<WindowPreviewView>(window_, /*exclude_shadow=*/true);
  preview_view->SetPreferredSize(GetPreviewSize(window_));
  preview_view_ = AddChildView(std::move(preview_view));
}

void ShelfWindowPreviewBubble::RemovePreviewView() {
  if (preview_view_) {
    WindowPreviewView* to_remove = preview_view_.ExtractAsDangling();
    RemoveChildViewT(to_remove);
  }
}

void ShelfWindowPreviewBubble::ProcessPressedEvent(ui::LocatedEvent* event) {
  if (!IsPointOverBubble(event->target()->GetScreenLocation(*event))) {
    return;
  }
  CHECK(dismiss_callback_);
  aura::Window* window = window_;
  aura::WindowTracker tracker({window});
  event->SetHandled();
  event->StopPropagation();
  RemovePreTargetHandler();
  // Running `dismiss_callback_` may synchronously close the widget and delete
  // `this` (e.g. via `ShelfMenuModelAdapter::Cancel`), so do not access any
  // members after this call.
  std::move(dismiss_callback_).Run();
  if (tracker.Contains(window)) {
    ::wm::ActivateWindow(window);
  }
}

void ShelfWindowPreviewBubble::AddPreTargetHandler() {
  if (!is_pre_target_handler_ && Shell::HasInstance()) {
    // Use `kSystem` priority so this handler runs ahead of `MenuHost` capture
    // and `ShelfTooltipManager` (`kDefault`).
    Shell::Get()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
    is_pre_target_handler_ = true;
  }
}

void ShelfWindowPreviewBubble::RemovePreTargetHandler() {
  gesture_consumer_.reset();
  if (is_pre_target_handler_ && Shell::HasInstance()) {
    Shell::Get()->RemovePreTargetHandler(this);
    is_pre_target_handler_ = false;
  }
}

BEGIN_METADATA(ShelfWindowPreviewBubble)
END_METADATA

}  // namespace ash
