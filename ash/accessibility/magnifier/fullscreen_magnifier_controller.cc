// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/accessibility/magnifier/magnifier_utils.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/root_window_transformers.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/root_window_transformer.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr float kMaxMagnifiedScale = 20.0f;
constexpr float kMinMagnifiedScaleThreshold = 1.1f;
constexpr float kNonMagnifiedScale = 1.0f;

constexpr float kInitialMagnifiedScale = 2.0f;
constexpr float kScrollScaleChangeFactor = 0.00125f;

// Default animation parameters for redrawing the magnification window.
constexpr gfx::Tween::Type kDefaultAnimationTweenType = gfx::Tween::EASE_OUT;
constexpr int kDefaultAnimationDurationInMs = 100;

// Use linear transformation to make the magnifier window move smoothly
// to center the focus when user types in a text input field.
constexpr gfx::Tween::Type kCenterCaretAnimationTweenType = gfx::Tween::LINEAR;

// Threshold of panning. If the cursor moves to within pixels (in DIP) of
// |kCursorPanningMargin| from the edge, the view-port moves.
constexpr int kCursorPanningMargin = 100;

// Threshold of panning at the bottom when the virtual keyboard is up. If the
// cursor moves to within pixels (in DIP) of |kKeyboardBottomPanningMargin| from
// the bottom edge, the view-port moves. This is only used by
// MoveMagnifierWindowFollowPoint() when |reduce_bottom_margin| is true.
constexpr int kKeyboardBottomPanningMargin = 10;

}  // namespace

class FullscreenMagnifierController::GestureProviderClient
    : public ui::GestureProviderAuraClient {
 public:
  GestureProviderClient() = default;
  GestureProviderClient(const GestureProviderClient&) = delete;
  GestureProviderClient& operator=(const GestureProviderClient&) = delete;
  ~GestureProviderClient() override = default;

  // ui::GestureProviderAuraClient overrides:
  void OnGestureEvent(GestureConsumer* consumer,
                      ui::GestureEvent* event) override {
    // Do nothing. OnGestureEvent is for timer based gesture events, e.g. tap.
    // FullscreenMagnifierController is interested only in pinch and scroll
    // gestures.
    DCHECK_NE(ui::EventType::kGestureScrollBegin, event->type());
    DCHECK_NE(ui::EventType::kGestureScrollEnd, event->type());
    DCHECK_NE(ui::EventType::kGestureScrollUpdate, event->type());
    DCHECK_NE(ui::EventType::kGesturePinchBegin, event->type());
    DCHECK_NE(ui::EventType::kGesturePinchEnd, event->type());
    DCHECK_NE(ui::EventType::kGesturePinchUpdate, event->type());
  }
};

FullscreenMagnifierController::FullscreenMagnifierController()
    : root_window_(Shell::GetPrimaryRootWindow()),
      scale_(kNonMagnifiedScale),
      original_scale_(kNonMagnifiedScale) {
  Shell::Get()->AddAccessibilityEventHandler(
      this,
      AccessibilityEventHandlerManager::HandlerType::kFullscreenMagnifier);
  root_window_->AddObserver(this);
  root_window_->GetHost()->GetEventSource()->AddEventRewriter(this);

  point_of_interest_in_root_ = root_window_->bounds().CenterPoint();

  gesture_provider_client_ = std::make_unique<GestureProviderClient>();
  gesture_provider_ = std::make_unique<ui::GestureProviderAura>(
      this, gesture_provider_client_.get());

  magnifier_debug_draw_rect_ = ::switches::IsMagnifierDebugDrawRectEnabled();
}

FullscreenMagnifierController::~FullscreenMagnifierController() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
  root_window_->RemoveObserver(this);

  Shell::Get()->RemoveAccessibilityEventHandler(this);
}

void FullscreenMagnifierController::SetEnabled(bool enabled) {
  if (enabled) {
    Shell* shell = Shell::Get();
    float scale =
        shell->accessibility_delegate()->GetSavedScreenMagnifierScale();
    if (scale <= 0.0f)
      scale = kInitialMagnifiedScale;
    ValidateScale(&scale);

    // Do nothing, if already enabled with same scale.
    if (is_enabled_ && scale == scale_)
      return;

    is_enabled_ = enabled;
    RedrawKeepingMousePosition(scale, true, false);
    shell->accessibility_delegate()->SaveScreenMagnifierScale(scale);
  } else {
    // Do nothing, if already disabled.
    if (!is_enabled_)
      return;

    RedrawKeepingMousePosition(kNonMagnifiedScale, true, false);
    is_enabled_ = enabled;
  }

  Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->OnFullscreenMagnifierEnabled(enabled);

  // Keyboard overscroll creates layout issues with fullscreen magnification
  // so it needs to be disabled when magnification is enabled.
  // TODO(spqchan): Fix the keyboard overscroll issues.
  auto config = keyboard::KeyboardUIController::Get()->keyboard_config();
  config.overscroll_behavior =
      is_enabled_ ? keyboard::KeyboardOverscrollBehavior::kDisabled
                  : keyboard::KeyboardOverscrollBehavior::kDefault;
  keyboard::KeyboardUIController::Get()->UpdateKeyboardConfig(config);
}

bool FullscreenMagnifierController::IsEnabled() const {
  return is_enabled_;
}

void FullscreenMagnifierController::SetScale(float scale, bool animate) {
  if (!is_enabled_)
    return;

  ValidateScale(&scale);
  Shell::Get()->accessibility_delegate()->SaveScreenMagnifierScale(scale);
  RedrawKeepingMousePosition(scale, animate, false);
}

void FullscreenMagnifierController::StepToNextScaleValue(int delta_index) {
  SetScale(magnifier_utils::GetNextMagnifierScaleValue(
               delta_index, GetScale(), kNonMagnifiedScale, kMaxMagnifiedScale),
           true /* animate */);
}

void FullscreenMagnifierController::MoveWindow(int x, int y, bool animate) {
  if (!is_enabled_)
    return;

  Redraw(gfx::PointF(x, y), scale_, animate);
}

void FullscreenMagnifierController::MoveWindow(const gfx::Point& point,
                                               bool animate) {
  if (!is_enabled_)
    return;

  Redraw(gfx::PointF(point), scale_, animate);
}

gfx::Point FullscreenMagnifierController::GetWindowPosition() const {
  return gfx::ToFlooredPoint(origin_);
}

void FullscreenMagnifierController::SetScrollDirection(
    ScrollDirection direction) {
  scroll_direction_ = direction;
  StartOrStopScrollIfNecessary();
}

gfx::Rect FullscreenMagnifierController::GetViewportRect() const {
  return gfx::ToEnclosingRect(GetWindowRectDIP(scale_));
}

void FullscreenMagnifierController::CenterOnPoint(
    const gfx::Point& point_in_screen) {
  gfx::Point point_in_root = point_in_screen;
  ::wm::ConvertPointFromScreen(root_window_, &point_in_root);

  MoveMagnifierWindowCenterPoint(point_in_root);
}

void FullscreenMagnifierController::HandleMoveMagnifierToRect(
    const gfx::Rect& rect_in_screen) {
  gfx::Rect node_bounds_in_root = rect_in_screen;
  ::wm::ConvertRectFromScreen(root_window_, &node_bounds_in_root);
  if (GetViewportRect().Contains(node_bounds_in_root))
    return;

  // Hide the cursor since this can cause jumps.
  Shell::Get()->cursor_manager()->HideCursor();
  MoveMagnifierWindowFollowRect(node_bounds_in_root);
}

void FullscreenMagnifierController::SwitchTargetRootWindow(
    aura::Window* new_root_window,
    bool redraw_original_root_window) {
  DCHECK(new_root_window);

  if (new_root_window == root_window_)
    return;

  // Stores the previous scale.
  float scale = GetScale();

  // Unmagnify the previous root window.
  root_window_->RemoveObserver(this);
  // TODO: This may need to remove the IME observer from the old root window
  // and add it to the new root window. https://crbug.com/820464

  // Do not move mouse back to its original position (point at border of the
  // root window) after redrawing as doing so will trigger root window switch
  // again.
  if (redraw_original_root_window)
    RedrawKeepingMousePosition(1.0f, true, true);
  root_window_ = new_root_window;
  RedrawKeepingMousePosition(scale, true, true);

  root_window_->AddObserver(this);
}

gfx::Transform FullscreenMagnifierController::GetMagnifierTransform() const {
  gfx::Transform transform;
  if (IsEnabled()) {
    transform.Scale(scale_, scale_);
    gfx::Point offset = GetWindowPosition();
    transform.Translate(-offset.x(), -offset.y());
  }

  return transform;
}

void FullscreenMagnifierController::OnImplicitAnimationsCompleted() {
  if (move_cursor_after_animation_) {
    MoveCursorTo(position_after_animation_);
    move_cursor_after_animation_ = false;

    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(root_window_);
    if (cursor_client)
      cursor_client->EnableMouseEvents();
  }

  is_on_animation_ = false;

  StartOrStopScrollIfNecessary();
}

void FullscreenMagnifierController::OnWindowDestroying(
    aura::Window* root_window) {
  if (root_window == root_window_) {
    // There must be at least one root window because this controller is
    // destroyed before the root windows get destroyed.
    DCHECK(root_window);

    aura::Window* target_root_window = Shell::GetRootWindowForNewWindows();
    CHECK(target_root_window);

    // The destroyed root window must not be target.
    CHECK_NE(target_root_window, root_window);
    // Don't redraw the old root window as it's being destroyed.
    SwitchTargetRootWindow(target_root_window, false);
    point_of_interest_in_root_ = target_root_window->bounds().CenterPoint();
  }
}

void FullscreenMagnifierController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
}

void FullscreenMagnifierController::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* current_root = target->GetRootWindow();
  gfx::PointF root_location_f = event->root_location_f();

  // Used for screen bounds checking.
  gfx::Point root_location = event->root_location();

  if (event->type() == ui::EventType::kMouseDragged) {
    auto* screen = display::Screen::GetScreen();
    const gfx::Point cursor_screen_location = screen->GetCursorScreenPoint();

    auto* window = screen->GetWindowAtScreenPoint(cursor_screen_location);
    // Update the |current_root| to be the one that contains the cursor
    // currently. This will make sure the magnifier be activated in the display
    // that contains the cursor while drag a window across displays.
    current_root =
        window ? window->GetRootWindow() : Shell::GetPrimaryRootWindow();
    root_location = cursor_screen_location;
    wm::ConvertPointFromScreen(current_root, &root_location);
    root_location_f = gfx::PointF(root_location);
  }

  if (current_root->bounds().Contains(root_location)) {
    // This must be before |SwitchTargetRootWindow()|.
    if (event->type() != ui::EventType::kMouseCaptureChanged) {
      point_of_interest_in_root_ = root_location;
    }

    if (current_root != root_window_) {
      DCHECK(current_root);
      SwitchTargetRootWindow(current_root, true);
    }

    const bool dragged_or_moved = event->type() == ui::EventType::kMouseMoved ||
                                  event->type() == ui::EventType::kMouseDragged;
    if (IsMagnified() && dragged_or_moved &&
        event->pointer_details().pointer_type != ui::EventPointerType::kPen) {
      OnMouseMove(root_location_f);
    }
  }
}

void FullscreenMagnifierController::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->IsAltDown() && event->IsControlDown()) {
    if (event->type() == ui::EventType::kScrollFlingStart) {
      event->StopPropagation();
      return;
    } else if (event->type() == ui::EventType::kScrollFlingCancel) {
      float scale = GetScale();
      // Jump back to exactly 1.0 if we are just a tiny bit zoomed in.
      // TODO(katie): These events are not fired after every scroll, which means
      // we don't always jump back to 1.0. Look into why they are missing.
      if (scale < kMinMagnifiedScaleThreshold) {
        scale = kNonMagnifiedScale;
        SetScale(scale, true);
      }
      event->StopPropagation();
      return;
    }

    if (event->type() == ui::EventType::kScroll) {
      SetScale(magnifier_utils::GetScaleFromScroll(
                   event->y_offset() * kScrollScaleChangeFactor, GetScale(),
                   kMaxMagnifiedScale, kNonMagnifiedScale),
               false /* animate */);
      event->StopPropagation();
      return;
    }
  }
}

void FullscreenMagnifierController::OnTouchEvent(ui::TouchEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* current_root = target->GetRootWindow();

  gfx::Rect root_bounds = current_root->bounds();
  if (!root_bounds.Contains(event->root_location()))
    return;

  point_of_interest_in_root_ = event->root_location();

  if (current_root != root_window_)
    SwitchTargetRootWindow(current_root, true);
}

ui::EventDispatchDetails FullscreenMagnifierController::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!IsEnabled())
    return SendEvent(continuation, &event);

  if (!event.IsTouchEvent())
    return SendEvent(continuation, &event);

  const ui::TouchEvent* touch_event = event.AsTouchEvent();

  if (touch_event->type() == ui::EventType::kTouchPressed) {
    touch_points_++;
    press_event_map_[touch_event->pointer_details().id] =
        std::make_unique<ui::TouchEvent>(*touch_event);
  } else if (touch_event->type() == ui::EventType::kTouchReleased ||
             touch_event->type() == ui::EventType::kTouchCancelled) {
    touch_points_--;
    press_event_map_.erase(touch_event->pointer_details().id);
  }

  ui::TouchEvent touch_event_copy = *touch_event;
  if (gesture_provider_->OnTouchEvent(&touch_event_copy)) {
    gesture_provider_->OnTouchEventAck(
        touch_event_copy.unique_event_id(), false /* event_consumed */,
        false /* is_source_touch_event_set_blocking */);
  } else {
    return DiscardEvent(continuation);
  }

  // User can change zoom level with two fingers pinch and pan around with two
  // fingers scroll. Once FullscreenMagnifierController detects one of those two
  // gestures, it starts consuming all touch events with cancelling existing
  // touches. If cancel_pressed_touches is set to true,
  // EventType::kTouchCancelled events are dispatched for existing touches after
  // the next for-loop.
  bool cancel_pressed_touches = ProcessGestures();

  if (cancel_pressed_touches) {
    DCHECK_EQ(2u, press_event_map_.size());

    // FullscreenMagnifierController starts consuming all touch events after it
    // cancells existing touches.
    consume_touch_event_ = true;

    for (const auto& it : press_event_map_) {
      ui::TouchEvent touch_cancel_event(ui::EventType::kTouchCancelled,
                                        gfx::Point(), touch_event->time_stamp(),
                                        it.second->pointer_details());
      touch_cancel_event.set_location_f(it.second->location_f());
      touch_cancel_event.set_root_location_f(it.second->root_location_f());
      touch_cancel_event.SetFlags(it.second->flags());

      // TouchExplorationController is watching event stream and managing its
      // internal state. If an event rewriter (FullscreenMagnifierController)
      // rewrites event stream, the next event rewriter won't get the event,
      // which makes TouchExplorationController confused. Send cancelled event
      // for recorded touch events to the next event rewriter here instead of
      // rewriting an event in the stream.
      ui::EventDispatchDetails details =
          SendEvent(continuation, &touch_cancel_event);
      if (details.dispatcher_destroyed || details.target_destroyed)
        return details;
    }
    press_event_map_.clear();
  }
  bool discard = consume_touch_event_;

  // Reset state once no point is touched on the screen.
  if (touch_points_ == 0) {
    consume_touch_event_ = false;

    // Jump back to exactly 1.0 if we are just a tiny bit zoomed in.
    if (scale_ < kMinMagnifiedScaleThreshold) {
      SetScale(kNonMagnifiedScale, true /* animate */);
    } else {
      // Store current magnifier scale in pref. We don't need to call this if we
      // call SetScale (the above case) as SetScale does this.
      Shell::Get()->accessibility_delegate()->SaveScreenMagnifierScale(scale_);
    }
  }

  if (discard)
    return DiscardEvent(continuation);

  return SendEvent(continuation, &event);
}

const std::string& FullscreenMagnifierController::GetName() const {
  static const std::string name("FullscreenMagnifierController");
  return name;
}

bool FullscreenMagnifierController::Redraw(
    const gfx::PointF& position_in_physical_pixels,
    float scale,
    bool animate) {
  gfx::PointF position =
      gfx::ConvertPointToDips(position_in_physical_pixels,
                              root_window_->layer()->device_scale_factor());
  return RedrawDIP(position, scale, animate ? kDefaultAnimationDurationInMs : 0,
                   kDefaultAnimationTweenType);
}

bool FullscreenMagnifierController::RedrawDIP(
    const gfx::PointF& position_in_dip,
    float scale,
    int duration_in_ms,
    gfx::Tween::Type tween_type) {
  DCHECK(root_window_);

  float x = position_in_dip.x();
  float y = position_in_dip.y();

  ValidateScale(&scale);

  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;

  const gfx::Size host_size_in_dip = GetHostSizeDIP();
  const gfx::SizeF window_size_in_dip = GetWindowRectDIP(scale).size();
  float max_x = host_size_in_dip.width() - window_size_in_dip.width();
  float max_y = host_size_in_dip.height() - window_size_in_dip.height();
  if (x > max_x)
    x = max_x;
  if (y > max_y)
    y = max_y;

  // Does nothing if both the origin and the scale are not changed.
  // Cast origin points back to int, as viewport can only be integer values.
  if (static_cast<int>(origin_.x()) == static_cast<int>(x) &&
      static_cast<int>(origin_.y()) == static_cast<int>(y) && scale == scale_) {
    return false;
  }

  origin_.set_x(x);
  origin_.set_y(y);
  scale_ = scale;

  const ui::LayerAnimator::PreemptionStrategy strategy =
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET;
  const base::TimeDelta duration = base::Milliseconds(duration_in_ms);

  ui::ScopedLayerAnimationSettings root_layer_settings(
      root_window_->layer()->GetAnimator());
  root_layer_settings.AddObserver(this);
  root_layer_settings.SetPreemptionStrategy(strategy);
  root_layer_settings.SetTweenType(tween_type);
  root_layer_settings.SetTransitionDuration(duration);

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_);
  std::unique_ptr<RootWindowTransformer> transformer(
      CreateRootWindowTransformerForDisplay(display));

  // Inverse the transformation on the keyboard container and display
  // identification highlight so the keyboard will remain zoomed out and the
  // highlight will render around the edges of the display. Apply the same
  // animation settings to it. Note: if |scale_| is 1.0f, the transform matrix
  // will be an identity matrix. Applying the inverse of an identity matrix will
  // not change the transformation.
  // TODO(spqchan): Find a way to sync the layer animations together.
  gfx::Transform inverse_transform;
  if (GetMagnifierTransform().GetInverse(&inverse_transform)) {
    std::vector<aura::Window*> undo_transform_windows = {
        root_window_->GetChildById(kShellWindowId_ImeWindowParentContainer)};

    aura::Window* display_identification_highlight =
        root_window_->GetChildById(kShellWindowId_ScreenAnimationContainer)
            ->GetChildById(kShellWindowId_DisplayIdentificationHighlightWindow);

    if (display_identification_highlight)
      undo_transform_windows.push_back(display_identification_highlight);

    for (auto* window : undo_transform_windows) {
      ui::ScopedLayerAnimationSettings layer_settings(
          window->layer()->GetAnimator());
      layer_settings.SetPreemptionStrategy(strategy);
      layer_settings.SetTweenType(tween_type);
      layer_settings.SetTransitionDuration(duration);
      window->SetTransform(inverse_transform);
    }
  }

  if (!magnifier_debug_draw_rect_) {
    RootWindowController::ForWindow(root_window_)
        ->ash_host()
        ->SetRootWindowTransformer(std::move(transformer));
  }

  if (duration_in_ms > 0)
    is_on_animation_ = true;

  Shell::Get()->accessibility_controller()->MagnifierBoundsChanged(
      GetViewportRect());

  return true;
}

void FullscreenMagnifierController::StartOrStopScrollIfNecessary() {
  // This value controls the scrolling speed.
  const int kMoveOffset = 40;
  if (is_on_animation_) {
    if (scroll_direction_ == SCROLL_NONE)
      root_window_->layer()->GetAnimator()->StopAnimating();
    return;
  }

  gfx::PointF new_origin = origin_;
  switch (scroll_direction_) {
    case SCROLL_NONE:
      // No need to take action.
      return;
    case SCROLL_LEFT:
      new_origin.Offset(-kMoveOffset, 0);
      break;
    case SCROLL_RIGHT:
      new_origin.Offset(kMoveOffset, 0);
      break;
    case SCROLL_UP:
      new_origin.Offset(0, -kMoveOffset);
      break;
    case SCROLL_DOWN:
      new_origin.Offset(0, kMoveOffset);
      break;
  }
  RedrawDIP(new_origin, scale_, kDefaultAnimationDurationInMs,
            kDefaultAnimationTweenType);
}

void FullscreenMagnifierController::RedrawKeepingMousePosition(
    float scale,
    bool animate,
    bool ignore_mouse_change) {
  gfx::Point mouse_in_root = point_of_interest_in_root_;
  // mouse_in_root is invalid value when the cursor is hidden.
  if (!root_window_->bounds().Contains(mouse_in_root))
    mouse_in_root = root_window_->bounds().CenterPoint();

  const gfx::PointF origin = gfx::PointF(
      mouse_in_root.x() - (scale_ / scale) * (mouse_in_root.x() - origin_.x()),
      mouse_in_root.y() - (scale_ / scale) * (mouse_in_root.y() - origin_.y()));
  bool changed =
      RedrawDIP(origin, scale, animate ? kDefaultAnimationDurationInMs : 0,
                kDefaultAnimationTweenType);
  if (!ignore_mouse_change && changed)
    AfterAnimationMoveCursorTo(mouse_in_root);
}

void FullscreenMagnifierController::OnMouseMove(
    const gfx::PointF& location_in_dip) {
  DCHECK(root_window_);

  gfx::Point center_point_in_dip(std::round(location_in_dip.x()),
                                 std::round(location_in_dip.y()));
  int margin = kCursorPanningMargin / scale_;  // No need to consider DPI.

  // Edge mouse following mode.
  int x_margin = margin;
  int y_margin = margin;

  if (mouse_following_mode_ == MagnifierMouseFollowingMode::kCentered ||
      mouse_following_mode_ == MagnifierMouseFollowingMode::kContinuous) {
    const gfx::Rect window_rect = GetViewportRect();
    x_margin = window_rect.width() / 2;
    y_margin = window_rect.height() / 2;
  }

  if (mouse_following_mode_ == MagnifierMouseFollowingMode::kContinuous) {
    // Continuous mouse panning mode is similar to centered mouse panning mode,
    // in that the screen moves behind the cursor when the user moves the mouse.
    // Unlike centered mouse panning mode however, the cursor is not centered in
    // the middle of the screen, but is able to freely move around, with the
    // screen moving in the opposite direction; for example, when the cursor
    // approaches the top left corner, the screen also scrolls behind it, so
    // that more of the top left portion of the screen is visible, until the
    // cursor reaches and meets up with the corner of the screen. This logic
    // calculates where the center point of the magnified region should be,
    // such that where the cursor is located in the magnified region corresponds
    // in proportion to where the cursor is located on the screen overall.

    // Screen size.
    const gfx::Size host_size_in_dip = GetHostSizeDIP();

    // Mouse position.
    const float x = location_in_dip.x();
    const float y = location_in_dip.y();

    // Viewport dimensions for calculation, increased by variable padding:
    // The cursor can never reach the bottom or right of the screen, it's always
    // at least one DIP away so that you can see it. (Note the cursor can reach
    // the top left at (0, 0)). Calculate the viewport size, adding some scaled
    // viewport padding as we move down and right so that the padding 0 in the
    // top/left and greater in the bottom right to account for the cursor not
    // being able to access the bottom corner.
    const float height =
        host_size_in_dip.height() / scale_ + 5 * y / host_size_in_dip.height();
    const float width =
        host_size_in_dip.width() / scale_ + 5 * x / host_size_in_dip.width();

    // The viewport center point is the mouse center point, minus the scaled
    // mouse center point to get to the viewport left/top edge, plus half
    // the viewport size.
    // In the example below, the host size is 12 units in width, the
    // mouse point x is at 7, and the viewport width is 3 (scale is 4.0).
    // The center_point_in_dip_x should be 6, with some integer rounding.
    // 6 = int(7 - (7 / 4.0) + (3 / 2.0))
    //  ____________
    // |            | host
    // |     ___    |
    // |    |  *|   |  <-- mouse x = 7, viewport width = 3
    // |    |___|   |
    // |____________|
    //  012345678901   <-- Indexes
    const int center_point_in_dip_x = x - x / scale_ + width / 2.0;
    const int center_point_in_dip_y = y - y / scale_ + height / 2.0;
    center_point_in_dip = {center_point_in_dip_x, center_point_in_dip_y};
  }

  // Reduce the bottom margin if the keyboard is visible.
  bool reduce_bottom_margin =
      keyboard::KeyboardUIController::Get()->IsKeyboardVisible();

  MoveMagnifierWindowFollowPoint(center_point_in_dip, x_margin, y_margin,
                                 reduce_bottom_margin);
}

void FullscreenMagnifierController::AfterAnimationMoveCursorTo(
    const gfx::Point& location) {
  DCHECK(root_window_);

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_);
  if (cursor_client) {
    // When cursor is invisible, do not move or show the cursor after the
    // animation.
    if (!cursor_client->IsCursorVisible())
      return;
    cursor_client->DisableMouseEvents();
  }
  move_cursor_after_animation_ = true;
  position_after_animation_ = location;
}

bool FullscreenMagnifierController::IsMagnified() const {
  return scale_ >= kMinMagnifiedScaleThreshold;
}

gfx::RectF FullscreenMagnifierController::GetWindowRectDIP(float scale) const {
  const gfx::Size size_in_dip = GetHostSizeDIP();
  const float width = size_in_dip.width() / scale;
  const float height = size_in_dip.height() / scale;

  return gfx::RectF(origin_.x(), origin_.y(), width, height);
}

gfx::Size FullscreenMagnifierController::GetHostSizeDIP() const {
  return root_window_->bounds().size();
}

void FullscreenMagnifierController::ValidateScale(float* scale) {
  *scale = std::clamp(*scale, kNonMagnifiedScale, kMaxMagnifiedScale);
  DCHECK(kNonMagnifiedScale <= *scale && *scale <= kMaxMagnifiedScale);
}

bool FullscreenMagnifierController::ProcessGestures() {
  bool cancel_pressed_touches = false;

  std::vector<std::unique_ptr<ui::GestureEvent>> gestures =
      gesture_provider_->GetAndResetPendingGestures();
  for (const auto& gesture : gestures) {
    const ui::GestureEventDetails& details = gesture->details();

    if (details.touch_points() != 2)
      continue;

    if (gesture->type() == ui::EventType::kGesturePinchBegin) {
      original_scale_ = scale_;

      // Start consuming touch events with cancelling existing touches.
      if (!consume_touch_event_)
        cancel_pressed_touches = true;
    } else if (gesture->type() == ui::EventType::kGesturePinchUpdate) {
      float scale = GetScale() * details.scale();
      ValidateScale(&scale);

      // |details.bounding_box().CenterPoint()| return center of touch points
      // of gesture in non-dip screen coordinate.
      gfx::PointF gesture_center =
          gfx::PointF(details.bounding_box().CenterPoint());

      // Root transform does dip scaling, screen magnification scaling and
      // translation. Apply inverse transform to convert non-dip screen
      // coordinate to dip logical coordinate.
      gesture_center =
          root_window_->GetHost()->GetInverseRootTransform().MapPoint(
              gesture_center);

      // Calcualte new origin to keep the distance between |gesture_center|
      // and |origin| same in screen coordinate. This means the following
      // equation.
      // (gesture_center.x - origin_.x) * scale_ =
      //   (gesture_center.x - new_origin.x) * scale
      // If you solve it for |new_origin|, you will get the following formula.
      const gfx::PointF origin = gfx::PointF(
          gesture_center.x() -
              (scale_ / scale) * (gesture_center.x() - origin_.x()),
          gesture_center.y() -
              (scale_ / scale) * (gesture_center.y() - origin_.y()));

      RedrawDIP(origin, scale, 0, kDefaultAnimationTweenType);
    } else if (gesture->type() == ui::EventType::kGestureScrollBegin) {
      original_origin_ = origin_;

      // Start consuming all touch events with cancelling existing touches.
      if (!consume_touch_event_)
        cancel_pressed_touches = true;
    } else if (gesture->type() == ui::EventType::kGestureScrollUpdate) {
      // The scroll offsets are apparently in pixels and does not take into
      // account the display rotation. Convert back to dip by applying the
      // inverse transform of the rotation (these are offsets, so we don't care
      // about scale or translation. We'll take care of the scale below).
      // https://crbug.com/867537.
      const auto display =
          display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_);
      gfx::Transform rotation_transform;
      rotation_transform.Rotate(display.PanelRotationAsDegree());
      gfx::Transform rotation_inverse_transform =
          rotation_transform.GetCheckedInverse();
      gfx::PointF scroll = rotation_inverse_transform.MapPoint(
          gfx::PointF(details.scroll_x(), details.scroll_y()));

      // Divide by scale to keep scroll speed same at any scale.
      float new_x = origin_.x() + (-scroll.x() / scale_);
      float new_y = origin_.y() + (-scroll.y() / scale_);

      RedrawDIP(gfx::PointF(new_x, new_y), scale_, 0,
                kDefaultAnimationTweenType);
    }
  }

  return cancel_pressed_touches;
}

void FullscreenMagnifierController::MoveMagnifierWindowFollowPoint(
    const gfx::Point& point,
    int x_margin,
    int y_margin,
    bool reduce_bottom_margin) {
  DCHECK(root_window_);
  bool start_zoom = false;

  // Current position.
  const gfx::Rect window_rect = GetViewportRect();
  const int top = window_rect.y();
  const int bottom = window_rect.bottom();

  int x_diff = 0;
  if (point.x() < window_rect.x() + x_margin) {
    // Panning left.
    x_diff = point.x() - (window_rect.x() + x_margin);
    start_zoom = true;
  } else if (point.x() > window_rect.right() - x_margin) {
    // Panning right.
    x_diff = point.x() - (window_rect.right() - x_margin);
    start_zoom = true;
  }
  int x = window_rect.x() + x_diff;

  // If |reduce_bottom_margin| is true, use kKeyboardBottomPanningMargin instead
  // of |y_margin|. This is to prevent the magnifier from panning when
  // the user is trying to interact with the bottom of the keyboard.
  const int bottom_panning_margin =
      reduce_bottom_margin ? kKeyboardBottomPanningMargin / scale_ : y_margin;

  int y_diff = 0;
  if (point.y() < top + y_margin) {
    // Panning up.
    y_diff = point.y() - (top + y_margin);
    start_zoom = true;
  } else if (bottom - bottom_panning_margin < point.y()) {
    // Panning down.
    const int bottom_target_margin =
        reduce_bottom_margin ? std::min(bottom_panning_margin, y_margin)
                             : y_margin;
    y_diff = point.y() - (bottom - bottom_target_margin);
    start_zoom = true;
  }
  int y = top + y_diff;
  if (start_zoom && !is_on_animation_) {
    bool ret = RedrawDIP(gfx::PointF(x, y), scale_,
                         0,  // No animation on panning.
                         kDefaultAnimationTweenType);

    if (ret &&
        mouse_following_mode_ != MagnifierMouseFollowingMode::kContinuous) {
      // If the magnified region is moved, hides the mouse cursor and moves it,
      // unless we're in continuous mode (in which case mouse position is
      // good already).
      if ((x_diff != 0 || y_diff != 0)) {
        MoveCursorTo(point);
      }
    }
  }
}

void FullscreenMagnifierController::MoveMagnifierWindowCenterPoint(
    const gfx::Point& point) {
  DCHECK(root_window_);

  gfx::Rect window_rect = GetViewportRect();

  // Reduce the viewport bounds if the keyboard is up.
  if (keyboard::KeyboardUIController::Get()->IsEnabled()) {
    gfx::Rect keyboard_rect = keyboard::KeyboardUIController::Get()
                                  ->GetKeyboardWindow()
                                  ->GetBoundsInScreen();
    window_rect.set_height(window_rect.height() -
                           keyboard_rect.height() / scale_);
  }

  if (point == window_rect.CenterPoint())
    return;

  if (!is_on_animation_) {
    // With animation on panning.
    RedrawDIP(
        gfx::PointF(window_rect.origin() + (point - window_rect.CenterPoint())),
        scale_, kDefaultAnimationDurationInMs, kCenterCaretAnimationTweenType);
  }
}

void FullscreenMagnifierController::MoveMagnifierWindowFollowRect(
    const gfx::Rect& rect) {
  DCHECK(root_window_);
  bool should_pan = false;

  const gfx::Rect viewport_rect = GetViewportRect();
  const int left = viewport_rect.x();
  const int right = viewport_rect.right();
  const gfx::Point rect_center = rect.CenterPoint();

  int x = left;
  if (rect.x() < left || right < rect.right()) {
    // Panning horizontally.
    x = rect_center.x() - viewport_rect.width() / 2;
    should_pan = true;
  }

  const int top = viewport_rect.y();
  const int bottom = viewport_rect.bottom();

  int y = top;
  if (rect.y() < top || bottom < rect.bottom()) {
    // Panning vertically.
    y = rect_center.y() - viewport_rect.height() / 2;
    should_pan = true;
  }

  // If rect is too wide to fit in viewport, include as much as we can, starting
  // with the left edge.
  if (rect.width() > viewport_rect.width())
    x = rect.x() - magnifier_utils::kLeftEdgeContextPadding;

  if (should_pan) {
    if (is_on_animation_) {
      root_window_->layer()->GetAnimator()->StopAnimating();
      is_on_animation_ = false;
    }
    RedrawDIP(gfx::PointF(x, y), scale_, kDefaultAnimationDurationInMs,
              kDefaultAnimationTweenType);
  }
}

void FullscreenMagnifierController::MoveCursorTo(
    const gfx::Point& root_location) {
  aura::WindowTreeHost* host = root_window_->GetHost();
  host->MoveCursorToLocationInPixels(gfx::ToCeiledPoint(
      host->GetRootTransform().MapPoint(gfx::PointF(root_location))));

  if (cursor_moved_callback_for_testing_) {
    cursor_moved_callback_for_testing_.Run(root_location);
  }
}

}  // namespace ash
