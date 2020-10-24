// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include <memory>
#include <utility>

#include "ash/drag_drop/drag_drop_tracker.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/animation/animation_delegate_notifier.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// The duration of the drag cancel animation in millisecond.
constexpr base::TimeDelta kCancelAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kTouchCancelAnimationDuration =
    base::TimeDelta::FromMilliseconds(20);
// The frame rate of the drag cancel animation in hertz.
const int kCancelAnimationFrameRate = 60;

// For touch initiated dragging, we scale and shift drag image by the following:
static const float kTouchDragImageScale = 1.2f;
static const int kTouchDragImageVerticalOffset = -25;

// Adjusts the drag image bounds such that the new bounds are scaled by |scale|
// and translated by the |drag_image_offset| and additional |vertical_offset|.
gfx::Rect AdjustDragImageBoundsForScaleAndOffset(
    const gfx::Rect& drag_image_bounds,
    int vertical_offset,
    float scale,
    gfx::Vector2d* drag_image_offset) {
  gfx::Point final_origin = drag_image_bounds.origin();
  gfx::SizeF final_size = gfx::SizeF(drag_image_bounds.size());
  final_size.Scale(scale);
  drag_image_offset->set_x(drag_image_offset->x() * scale);
  drag_image_offset->set_y(drag_image_offset->y() * scale);
  int total_x_offset = drag_image_offset->x();
  int total_y_offset = drag_image_offset->y() - vertical_offset;
  final_origin.Offset(-total_x_offset, -total_y_offset);
  return gfx::ToEnclosingRect(
      gfx::RectF(gfx::PointF(final_origin), final_size));
}

void DispatchGestureEndToWindow(aura::Window* window) {
  if (window && window->delegate()) {
    ui::GestureEventDetails details(ui::ET_GESTURE_END);
    details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_end(0, 0, 0, ui::EventTimeForNow(), details);
    window->delegate()->OnGestureEvent(&gesture_end);
  }
}

}  // namespace

class DragDropTrackerDelegate : public aura::WindowDelegate {
 public:
  explicit DragDropTrackerDelegate(DragDropController* controller)
      : drag_drop_controller_(controller) {}
  ~DragDropTrackerDelegate() override = default;

  // Overridden from WindowDelegate:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  gfx::Size GetMaximumSize() const override { return gfx::Size(); }

  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::kNullCursor;
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTCAPTION;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return true; }
  void OnCaptureLost() override {
    if (drag_drop_controller_->IsDragDropInProgress())
      drag_drop_controller_->DragCancel();
  }
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override {}
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return true; }
  void GetHitTestMask(SkPath* mask) const override { DCHECK(mask->isEmpty()); }

 private:
  DragDropController* drag_drop_controller_;

  DISALLOW_COPY_AND_ASSIGN(DragDropTrackerDelegate);
};

////////////////////////////////////////////////////////////////////////////////
// DragDropController, public:

DragDropController::DragDropController()
    : drag_drop_window_delegate_(new DragDropTrackerDelegate(this)) {
  Shell::Get()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
}

DragDropController::~DragDropController() {
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  Shell::Get()->RemovePreTargetHandler(this);
  Cleanup();
  if (cancel_animation_)
    cancel_animation_->End();
  drag_image_widget_.reset();
}

int DragDropController::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int operation,
    ui::mojom::DragEventSource source) {
  if (!enabled_ || IsDragDropInProgress())
    return 0;

  const ui::OSExchangeDataProvider* provider = &data->provider();
  // We do not support touch drag/drop without a drag image.
  if (source == ui::mojom::DragEventSource::kTouch &&
      provider->GetDragImage().size().IsEmpty())
    return 0;

  current_drag_event_source_ = source;
  DragDropTracker* tracker =
      new DragDropTracker(root_window, drag_drop_window_delegate_.get());
  if (source == ui::mojom::DragEventSource::kTouch) {
    // We need to transfer the current gesture sequence and the GR's touch event
    // queue to the |drag_drop_tracker_|'s capture window so that when it takes
    // capture, it still gets a valid gesture state.
    aura::Env::GetInstance()->gesture_recognizer()->TransferEventsTo(
        source_window, tracker->capture_window(),
        ui::TransferTouchesBehavior::kCancel);
    // We also send a gesture end to the source window so it can clear state.
    // TODO(varunjain): Remove this whole block when gesture sequence
    // transferring is properly done in the GR (http://crbug.com/160558)
    DispatchGestureEndToWindow(source_window);
  }
  tracker->TakeCapture();
  drag_drop_tracker_.reset(tracker);
  drag_source_window_ = source_window;
  if (drag_source_window_)
    drag_source_window_->AddObserver(this);
  pending_long_tap_.reset();

  drag_data_ = std::move(data);
  drag_operation_ = operation;
  current_drag_actions_ = 0;

  start_location_ = screen_location;
  current_location_ = screen_location;

  SetDragImage(provider->GetDragImage(), provider->GetDragImageOffset());

  drag_window_ = nullptr;

  // Ends cancel animation if it's in progress.
  if (cancel_animation_)
    cancel_animation_->End();

  for (aura::client::DragDropClientObserver& observer : observers_)
    observer.OnDragStarted();

  if (TabDragDropDelegate::IsChromeTabDrag(*drag_data_)) {
    DCHECK(!tab_drag_drop_delegate_);
    tab_drag_drop_delegate_.emplace(root_window, drag_source_window_,
                                    start_location_);
    static_cast<DragImageView*>(drag_image_widget_->GetContentsView())
        ->SetTouchDragOperationHintOff();
  }

  if (should_block_during_drag_drop_) {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  if (!cancel_animation_.get() || !cancel_animation_->is_animating() ||
      !pending_long_tap_.get()) {
    // If drag cancel animation is running, this cleanup is done when the
    // animation completes.
    if (drag_source_window_)
      drag_source_window_->RemoveObserver(this);
    drag_source_window_ = nullptr;
  }

  return drag_operation_;
}

void DragDropController::SetDragImage(const gfx::ImageSkia& image,
                                      const gfx::Vector2d& image_offset) {
  auto source = current_drag_event_source_;
  auto* source_window = drag_source_window_;

  float drag_image_scale = 1;
  int drag_image_vertical_offset = 0;
  if (source == ui::mojom::DragEventSource::kTouch) {
    drag_image_scale = kTouchDragImageScale;
    drag_image_vertical_offset = kTouchDragImageVerticalOffset;
  }
  drag_image_final_bounds_for_cancel_animation_ =
      gfx::Rect(start_location_ - image_offset, image.size());

  if (!drag_image_widget_) {
    drag_image_widget_ =
        DragImageView::Create(source_window->GetRootWindow(), source);
  }

  DragImageView* drag_image =
      static_cast<DragImageView*>(drag_image_widget_->GetContentsView());
  drag_image->SetImage(image);
  drag_image_offset_ = image_offset;
  gfx::Rect drag_image_bounds(current_location_,
                              drag_image->GetPreferredSize());
  drag_image_bounds = AdjustDragImageBoundsForScaleAndOffset(
      drag_image_bounds, drag_image_vertical_offset, drag_image_scale,
      &drag_image_offset_);
  drag_image->SetBoundsInScreen(drag_image_bounds);
  drag_image->SetWidgetVisible(true);
  if (source == ui::mojom::DragEventSource::kTouch) {
    drag_image->SetTouchDragOperationHintPosition(
        gfx::Point(drag_image_offset_.x(),
                   drag_image_offset_.y() + drag_image_vertical_offset));
  }
}

void DragDropController::DragCancel() {
  DCHECK(enabled_);
  DoDragCancel(kCancelAnimationDuration);
}

bool DragDropController::IsDragDropInProgress() {
  return !!drag_drop_tracker_ && !!drag_data_;
}

void DragDropController::AddObserver(
    aura::client::DragDropClientObserver* observer) {
  observers_.AddObserver(observer);
}

void DragDropController::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DragDropController::OnKeyEvent(ui::KeyEvent* event) {
  if (IsDragDropInProgress() && event->key_code() == ui::VKEY_ESCAPE) {
    DragCancel();
    event->StopPropagation();
  }
}

void DragDropController::OnMouseEvent(ui::MouseEvent* event) {
  if (!IsDragDropInProgress())
    return;

  // If current drag session was not started by mouse, dont process this mouse
  // event, but consume it so it does not interfere with current drag session.
  if (current_drag_event_source_ != ui::mojom::DragEventSource::kMouse) {
    event->StopPropagation();
    return;
  }

  aura::Window* translated_target = drag_drop_tracker_->GetTarget(*event);
  if (!translated_target) {
    DragCancel();
    event->StopPropagation();
    return;
  }
  std::unique_ptr<ui::LocatedEvent> translated_event(
      drag_drop_tracker_->ConvertEvent(translated_target, *event));
  switch (translated_event->type()) {
    case ui::ET_MOUSE_DRAGGED:
      DragUpdate(translated_target, *translated_event.get());
      break;
    case ui::ET_MOUSE_RELEASED:
      Drop(translated_target, *translated_event.get());
      break;
    default:
      // We could also reach here because RootWindow may sometimes generate a
      // bunch of fake mouse events
      // (aura::RootWindow::PostMouseMoveEventAfterWindowChange).
      break;
  }
  event->StopPropagation();
}

void DragDropController::OnTouchEvent(ui::TouchEvent* event) {
  if (!IsDragDropInProgress())
    return;

  // If current drag session was not started by touch, dont process this touch
  // event, but consume it so it does not interfere with current drag session.
  if (current_drag_event_source_ != ui::mojom::DragEventSource::kTouch)
    event->StopPropagation();

  if (event->handled())
    return;

  if (event->type() == ui::ET_TOUCH_CANCELLED)
    DragCancel();
}

void DragDropController::OnGestureEvent(ui::GestureEvent* event) {
  if (!IsDragDropInProgress())
    return;

  // No one else should handle gesture events when in drag drop. Note that it is
  // not enough to just set ER_HANDLED because the dispatcher only stops
  // dispatching when the event has ER_CONSUMED. If we just set ER_HANDLED, the
  // event will still be dispatched to other handlers and we depend on
  // individual handlers' kindness to not touch events marked ER_HANDLED (not
  // all handlers are so kind and may cause bugs like crbug.com/236493).
  event->StopPropagation();

  // If current drag session was not started by touch, dont process this event.
  if (current_drag_event_source_ != ui::mojom::DragEventSource::kTouch)
    return;

  // Apply kTouchDragImageVerticalOffset to the location.
  ui::GestureEvent touch_offset_event(*event,
                                      static_cast<aura::Window*>(nullptr),
                                      static_cast<aura::Window*>(nullptr));
  gfx::PointF touch_offset_location = touch_offset_event.location_f();
  gfx::PointF touch_offset_root_location = touch_offset_event.root_location_f();
  touch_offset_location.Offset(0, kTouchDragImageVerticalOffset);
  touch_offset_root_location.Offset(0, kTouchDragImageVerticalOffset);
  touch_offset_event.set_location_f(touch_offset_location);
  touch_offset_event.set_root_location_f(touch_offset_root_location);

  aura::Window* translated_target =
      drag_drop_tracker_->GetTarget(touch_offset_event);
  if (!translated_target) {
    DragCancel();
    event->SetHandled();
    return;
  }
  std::unique_ptr<ui::LocatedEvent> translated_event(
      drag_drop_tracker_->ConvertEvent(translated_target, touch_offset_event));

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_UPDATE:
      DragUpdate(translated_target, *translated_event.get());
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      Drop(translated_target, *translated_event.get());
      break;
    case ui::ET_GESTURE_LONG_TAP:
      // Ideally we would want to just forward this long tap event to the
      // |drag_source_window_|. However, webkit does not accept events while a
      // drag drop is still in progress. The drag drop ends only when the nested
      // message loop ends. Due to this stupidity, we have to defer forwarding
      // the long tap.
      pending_long_tap_.reset(new ui::GestureEvent(
          *event,
          static_cast<aura::Window*>(drag_drop_tracker_->capture_window()),
          static_cast<aura::Window*>(drag_source_window_)));
      DoDragCancel(kTouchCancelAnimationDuration);
      break;
    default:
      break;
  }
  event->SetHandled();
}

void DragDropController::OnWindowDestroyed(aura::Window* window) {
  if (drag_window_ == window)
    drag_window_ = nullptr;
  if (drag_source_window_ == window)
    drag_source_window_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// DragDropController, protected:

gfx::LinearAnimation* DragDropController::CreateCancelAnimation(
    base::TimeDelta duration,
    int frame_rate,
    gfx::AnimationDelegate* delegate) {
  return new gfx::LinearAnimation(duration, frame_rate, delegate);
}

void DragDropController::DragUpdate(aura::Window* target,
                                    const ui::LocatedEvent& event) {
  int op = ui::DragDropTypes::DRAG_NONE;
  if (target != drag_window_) {
    if (drag_window_) {
      aura::client::DragDropDelegate* delegate =
          aura::client::GetDragDropDelegate(drag_window_);
      if (delegate)
        delegate->OnDragExited();
      if (drag_window_ != drag_source_window_)
        drag_window_->RemoveObserver(this);
    }
    drag_window_ = target;
    // We are already an observer of |drag_source_window_| so no need to add.
    if (drag_window_ != drag_source_window_)
      drag_window_->AddObserver(this);
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(drag_window_);
    if (delegate) {
      ui::DropTargetEvent e(*drag_data_.get(), event.location_f(),
                            event.root_location_f(), drag_operation_);
      e.set_flags(event.flags());
      ui::Event::DispatcherApi(&e).set_target(target);
      delegate->OnDragEntered(e);
    }
  } else {
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(drag_window_);
    if (delegate) {
      ui::DropTargetEvent e(*drag_data_.get(), event.location_f(),
                            event.root_location_f(), drag_operation_);
      e.set_flags(event.flags());
      ui::Event::DispatcherApi(&e).set_target(target);
      op = delegate->OnDragUpdated(e);
      gfx::NativeCursor cursor = ui::mojom::CursorType::kNoDrop;
      if (op & ui::DragDropTypes::DRAG_COPY)
        cursor = ui::mojom::CursorType::kCopy;
      else if (op & ui::DragDropTypes::DRAG_LINK)
        cursor = ui::mojom::CursorType::kAlias;
      else if (op & ui::DragDropTypes::DRAG_MOVE)
        cursor = ui::mojom::CursorType::kGrabbing;

      Shell::Get()->cursor_manager()->SetCursor(cursor);
    }
  }

  if (op != current_drag_actions_) {
    current_drag_actions_ = op;

    for (aura::client::DragDropClientObserver& observer : observers_)
      observer.OnDragActionsChanged(op);
  }

  gfx::Point root_location_in_screen = event.root_location();
  ::wm::ConvertPointToScreen(target->GetRootWindow(), &root_location_in_screen);

  DCHECK(drag_image_widget_);
  DragImageView* drag_image =
      static_cast<DragImageView*>(drag_image_widget_->GetContentsView());
  if (drag_image->GetVisible()) {
    current_location_ = root_location_in_screen;
    drag_image->SetScreenPosition(root_location_in_screen - drag_image_offset_);
    drag_image->SetTouchDragOperation(op);
  }

  if (tab_drag_drop_delegate_) {
    // TabDragDropDelegate assumes the root window doesn't change. Tab drags are
    // only seen in tablet mode which precludes dragging between displays.
    // DCHECK just to make sure.
    DCHECK_EQ(target->GetRootWindow(), tab_drag_drop_delegate_->root_window());

    tab_drag_drop_delegate_->DragUpdate(root_location_in_screen);
  }
}

void DragDropController::Drop(aura::Window* target,
                              const ui::LocatedEvent& event) {
  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kPointer);

  // We must guarantee that a target gets a OnDragEntered before Drop. WebKit
  // depends on not getting a Drop without DragEnter. This behavior is
  // consistent with drag/drop on other platforms.
  if (target != drag_window_)
    DragUpdate(target, event);
  DCHECK(target == drag_window_);

  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(target);
  if (delegate) {
    ui::DropTargetEvent e(*drag_data_.get(), event.location_f(),
                          event.root_location_f(), drag_operation_);
    e.set_flags(event.flags());
    ui::Event::DispatcherApi(&e).set_target(target);

    ui::OSExchangeData copied_data(drag_data_->provider().Clone());
    drag_operation_ = delegate->OnPerformDrop(e, std::move(drag_data_));
    if (drag_operation_ == 0 && tab_drag_drop_delegate_) {
      gfx::Point location_in_screen = event.root_location();
      ::wm::ConvertPointToScreen(target->GetRootWindow(), &location_in_screen);
      tab_drag_drop_delegate_->Drop(location_in_screen, copied_data);
      StartCanceledAnimation(kCancelAnimationDuration);
    } else if (drag_operation_ == 0) {
      StartCanceledAnimation(kCancelAnimationDuration);
    } else {
      drag_image_widget_.reset();
    }
  } else {
    drag_image_widget_.reset();
  }

  Cleanup();
  if (should_block_during_drag_drop_)
    std::move(quit_closure_).Run();
}

////////////////////////////////////////////////////////////////////////////////
// DragDropController, private:

void DragDropController::AnimationEnded(const gfx::Animation* animation) {
  cancel_animation_.reset();
  cancel_animation_notifier_.reset();

  // By the time we finish animation, another drag/drop session may have
  // started. We do not want to destroy the drag image in that case.
  if (!IsDragDropInProgress())
    drag_image_widget_.reset();
  if (pending_long_tap_) {
    // If not in a nested run loop, we can forward the long tap right now.
    if (!should_block_during_drag_drop_) {
      ForwardPendingLongTap();
    } else {
      // See comment about this in OnGestureEvent().
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&DragDropController::ForwardPendingLongTap,
                                    weak_factory_.GetWeakPtr()));
    }
  }
}

void DragDropController::DoDragCancel(
    base::TimeDelta drag_cancel_animation_duration) {
  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kPointer);

  // |drag_window_| can be NULL if we have just started the drag and have not
  // received any DragUpdates, or, if the |drag_window_| gets destroyed during
  // a drag/drop.
  aura::client::DragDropDelegate* delegate =
      drag_window_ ? aura::client::GetDragDropDelegate(drag_window_) : nullptr;
  if (delegate)
    delegate->OnDragExited();

  Cleanup();
  drag_operation_ = 0;
  StartCanceledAnimation(drag_cancel_animation_duration);
  if (should_block_during_drag_drop_)
    std::move(quit_closure_).Run();
}

void DragDropController::AnimationProgressed(const gfx::Animation* animation) {
  gfx::Rect current_bounds = animation->CurrentValueBetween(
      drag_image_initial_bounds_for_cancel_animation_,
      drag_image_final_bounds_for_cancel_animation_);
  static_cast<DragImageView*>(drag_image_widget_->GetContentsView())
      ->SetBoundsInScreen(current_bounds);
}

void DragDropController::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void DragDropController::OnDisplayConfigurationChanging() {
  // Abort in-progress drags if a monitor is added or removed because the drag
  // image widget's container may be destroyed.
  if (IsDragDropInProgress())
    DragCancel();
}

void DragDropController::StartCanceledAnimation(
    base::TimeDelta animation_duration) {
  DCHECK(drag_image_widget_);
  DragImageView* drag_image =
      static_cast<DragImageView*>(drag_image_widget_->GetContentsView());
  drag_image->SetTouchDragOperationHintOff();
  drag_image_initial_bounds_for_cancel_animation_ =
      drag_image->GetBoundsInScreen();
  cancel_animation_notifier_ = std::make_unique<
      gfx::AnimationDelegateNotifier<views::AnimationDelegateViews>>(
      this, drag_image);
  cancel_animation_.reset(
      CreateCancelAnimation(animation_duration, kCancelAnimationFrameRate,
                            cancel_animation_notifier_.get()));
  cancel_animation_->Start();
}

void DragDropController::ForwardPendingLongTap() {
  if (drag_source_window_ && drag_source_window_->delegate()) {
    drag_source_window_->delegate()->OnGestureEvent(pending_long_tap_.get());
    DispatchGestureEndToWindow(drag_source_window_);
  }
  pending_long_tap_.reset();
  if (drag_source_window_)
    drag_source_window_->RemoveObserver(this);
  drag_source_window_ = nullptr;
}

void DragDropController::Cleanup() {
  for (aura::client::DragDropClientObserver& observer : observers_)
    observer.OnDragEnded();
  if (drag_window_)
    drag_window_->RemoveObserver(this);
  drag_window_ = nullptr;
  drag_data_.reset();

  tab_drag_drop_delegate_.reset();

  // Cleanup can be called again while deleting DragDropTracker, so delete
  // the pointer with a local variable to avoid double free.
  std::unique_ptr<DragDropTracker> holder = std::move(drag_drop_tracker_);
}

}  // namespace ash
