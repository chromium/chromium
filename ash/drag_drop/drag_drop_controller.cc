// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include <memory>
#include <utility>

#include "ash/drag_drop/drag_image_view.h"
#include "ash/drag_drop/toplevel_window_drag_delegate.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/hang_watcher.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/hit_test.h"
#include "ui/display/manager/display_manager.h"
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

using ::ui::mojom::DragOperation;

// The duration of the drag cancel animation in millisecond.
constexpr base::TimeDelta kCancelAnimationDuration = base::Milliseconds(250);
constexpr base::TimeDelta kTouchCancelAnimationDuration =
    base::Milliseconds(20);
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

void DropIfAllowed(const ui::OSExchangeData* drag_data,
                   aura::client::DragUpdateInfo& drag_info,
                   base::OnceClosure drop_cb) {
  DCHECK(drag_data);

  if (ui::DataTransferPolicyController::HasInstance()) {
    ui::DataTransferPolicyController::Get()->DropIfAllowed(
        (drag_data->GetSource() ? std::make_optional<ui::DataTransferEndpoint>(
                                      *drag_data->GetSource())
                                : std::nullopt),
        {drag_info.data_endpoint}, drag_data->GetFilenames(),
        std::move(drop_cb));
  } else {
    std::move(drop_cb).Run();
  }
}

std::unique_ptr<ui::LocatedEvent> ConvertEvent(aura::Window* target,
                                               const ui::LocatedEvent& event) {
  gfx::Point target_location = event.location();
  aura::Window::ConvertPointToTarget(static_cast<aura::Window*>(event.target()),
                                     target, &target_location);
  gfx::Point target_root_location = event.location();
  aura::Window* target_root = target->GetRootWindow();
  aura::Window::ConvertPointToTarget(static_cast<aura::Window*>(event.target()),
                                     target_root, &target_root_location);
  int changed_button_flags = 0;
  if (event.IsMouseEvent())
    changed_button_flags = event.AsMouseEvent()->changed_button_flags();
  return std::make_unique<ui::MouseEvent>(
      event.type(), target_location, target_root_location,
      ui::EventTimeForNow(), event.flags(), changed_button_flags);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DragDropController, public:

DragDropController::DragDropController() {
  Shell::Get()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
}

DragDropController::~DragDropController() {
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
  Shell::Get()->RemovePreTargetHandler(this);
  Cleanup();
  if (cancel_animation_)
    cancel_animation_->End();
  drag_image_widget_.reset();
  for (aura::client::DragDropClientObserver& observer : observers_) {
    observer.OnDragDropClientDestroying();
  }
}

bool DragDropController::IsDragDropCompleted() {
  return drag_drop_completed_;
}

DragOperation DragDropController::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int allowed_operations,
    ui::mojom::DragEventSource source) {
  if (!enabled_ || IsDragDropInProgress())
    return DragOperation::kNone;

  weak_factory_.InvalidateWeakPtrs();

  const ui::OSExchangeDataProvider* provider = &data->provider();

  // We do not support touch drag/drop without a drag image, unless it is a tab
  // drag/drop.
  if (source == ui::mojom::DragEventSource::kTouch &&
      (!allow_no_image_touch_drag_for_test_ &&
       provider->GetDragImage().size().IsEmpty()) &&
      !toplevel_window_drag_delegate_) {
    return DragOperation::kNone;
  }

  // Never consider the current scope as hung. The hang watching deadline (if
  // any) is not valid since the user can take unbounded time to complete the
  // drag.
  base::HangWatcher::InvalidateActiveExpectations();

  operation_ = DragOperation::kNone;
  current_drag_event_source_ = source;
  capture_delegate_ = nullptr;

  const bool is_touch_source = source == ui::mojom::DragEventSource::kTouch;
  bool touch_capture_attempted = false;

  // When an extended drag is started, a capture window will be created to
  // handle moving gestures between different wl surfaces to support dragging
  // chrome tabs into and out of browsers.
  if (is_touch_source && toplevel_window_drag_delegate_) {
    touch_capture_attempted = true;
    if (toplevel_window_drag_delegate_->TakeCapture(
            root_window, source_window,
            base::BindRepeating(&DragDropController::CancelIfInProgress,
                                base::Unretained(this)),
            ui::TransferTouchesBehavior::kCancel)) {
      capture_delegate_ = toplevel_window_drag_delegate_;
    }
  }

  // |drag_source_window_| and |pending_long_tap_| could be non-null if a new
  // drag starts while cancel animation or forwarding long tap event is ongoing.
  // In this case, forwarding long tap event is aborted and related state should
  // be cleaned up.
  CleanupPendingLongTap();

  drag_source_window_ = source_window;
  if (drag_source_window_)
    drag_source_window_->AddObserver(this);

  drag_drop_completed_ = false;
  drag_data_ = std::move(data);
  allowed_operations_ = allowed_operations;
  current_drag_info_ = aura::client::DragUpdateInfo();

  start_location_ = screen_location;
  current_location_ = screen_location;

  // Ends cancel animation if it's in progress.
  // This should happen before setting drag image because it refers to the drag
  // image.
  if (cancel_animation_)
    cancel_animation_->End();
  cancel_animation_.reset();

  SetDragImage(provider->GetDragImage(), provider->GetDragImageOffset());

  drag_window_ = nullptr;

  for (aura::client::DragDropClientObserver& observer : observers_)
    observer.OnDragStarted();

  if (toplevel_window_drag_delegate_) {
    toplevel_window_drag_delegate_->OnToplevelWindowDragStarted(
        gfx::PointF(start_location_), source, drag_source_window_);
  }

  if (TabDragDropDelegate::IsChromeTabDrag(*drag_data_)) {
    // TODO(aluh): Figure out why this allocation is outside the inner if-block.
    DCHECK(!tab_drag_drop_delegate_);
    tab_drag_drop_delegate_ = std::make_unique<TabDragDropDelegate>(
        root_window, drag_source_window_, start_location_);
    if (drag_image_widget_) {
      static_cast<DragImageView*>(drag_image_widget_->GetContentsView())
          ->SetTouchDragOperationHintOff();
    }
    // Avoid taking capture twice.
    if (is_touch_source && !touch_capture_attempted) {
      touch_capture_attempted = true;
      if (tab_drag_drop_delegate_->TakeCapture(
              root_window, source_window,
              base::BindRepeating(&DragDropController::CancelIfInProgress,
                                  base::Unretained(this)),
              ui::TransferTouchesBehavior::kDontCancel)) {
        capture_delegate_ = tab_drag_drop_delegate_.get();
      }
    }
  }
  // If touch is not captured by either extended drag nor tab drag, start
  // a normal drag-and-drop using DragDropCaptureDelegate.
  if (is_touch_source && !touch_capture_attempted) {
    touch_capture_attempted = true;
    // For other type of touch drag, use normal DDCaptureDelegate;
    touch_drag_drop_delegate_ = std::make_unique<DragDropCaptureDelegate>();
    if (touch_drag_drop_delegate_->TakeCapture(
            root_window, source_window,
            base::BindRepeating(&DragDropController::CancelIfInProgress,
                                base::Unretained(this)),
            ui::TransferTouchesBehavior::kDontCancel)) {
      capture_delegate_ = touch_drag_drop_delegate_.get();
    }
  }

  if (touch_capture_attempted && !capture_delegate_) {
    Cleanup();
  } else {
    if (test_loop_closure_) {
      while (!quit_closure_.is_null())
        test_loop_closure_.Run();
    } else {
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  if (!will_forward_long_tap_) {
    // If drag cancel animation or long tap event forwarding is running, this
    // cleanup is done when the long tap event forwarding is done, or when a new
    // drag is started.

    // A check to catch an UAF issue like crbug.com/1282480 on non asan build.
    DCHECK(!drag_source_window_ || !drag_source_window_->is_destroying());

    CleanupPendingLongTap();
  }

  return operation_;
}

void DragDropController::SetDragImage(const gfx::ImageSkia& image,
                                      const gfx::Vector2d& image_offset) {
  if (image.size().IsEmpty()) {
    drag_image_widget_.reset();
    drag_image_final_bounds_for_cancel_animation_ = gfx::Rect();
    drag_image_offset_ = gfx::Vector2d();
    return;
  }

  auto source = current_drag_event_source_;
  auto* source_window = drag_source_window_.get();

  float drag_image_scale = 1;
  int drag_image_vertical_offset = 0;
  if (source == ui::mojom::DragEventSource::kTouch) {
    drag_image_scale = kTouchDragImageScale;
    drag_image_vertical_offset = kTouchDragImageVerticalOffset;
  }
  drag_image_final_bounds_for_cancel_animation_ =
      gfx::Rect(start_location_ - image_offset, image.size());

  // Only create `drag_image_widget_` if it doesn't exist. This prevents the
  // case when dragging a webui tab in lacros keeps creating fresh
  // `drag_image_widget_` with kTouch while it should have been set as kMouse to
  // avoid drag hint. See crbug.com/1384469.
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

void DragDropController::SetLoopClosureForTesting(
    TestLoopClosure closure,
    base::OnceClosure quit_closure) {
  test_loop_closure_ = closure;
  quit_closure_ = std::move(quit_closure);
}

void DragDropController::SetDisableNestedLoopForTesting(bool disable) {
  nested_loop_disabled_for_testing_ = disable;
  if (disable) {
    base::OnceClosure quit_closure;
    SetLoopClosureForTesting(base::DoNothing(), std::move(quit_closure));
  } else {
    test_loop_closure_.Reset();
    quit_closure_.Reset();
  }
}

void DragDropController::DragCancel() {
  DCHECK(enabled_);
  DoDragCancel(kCancelAnimationDuration);
}

bool DragDropController::IsDragDropInProgress() {
  return !!drag_data_;
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
  aura::Window* translated_target =
      window_util::GetEventHandlerForEvent(*event);
  if (!translated_target) {
    // EventType::kMouseCaptureChanged event does not have a location that can
    // be used to locate a translated target.
    if (event->type() != ui::EventType::kMouseCaptureChanged) {
      DragCancel();
    }
    event->StopPropagation();
    return;
  }

  auto translated_event = ConvertEvent(translated_target, *event);
  switch (translated_event->type()) {
    case ui::EventType::kMouseDragged:
      DragUpdate(translated_target, *translated_event.get());
      break;
    case ui::EventType::kMouseReleased:
      Drop(translated_target, *translated_event.get());
      break;
    default:
      // We could also reach here because RootWindow may sometimes generate a
      // bunch of fake mouse events
      // (aura::RootWindow::PostMouseMoveEventAfterWindowChange).
      break;
  }

  if (toplevel_window_drag_delegate_)
    toplevel_window_drag_delegate_->OnToplevelWindowDragEvent(event);

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

  if (event->type() == ui::EventType::kTouchCancelled) {
    DragCancel();
  }
}

void DragDropController::OnGestureEvent(ui::GestureEvent* event) {
  if (!IsDragDropInProgress())
    return;

  // If current drag session was not started by touch, dont process this event
  // but consume it so it does not interfere with current drag session.
  if (current_drag_event_source_ != ui::mojom::DragEventSource::kTouch) {
    event->StopPropagation();
    return;
  }

  // Apply kTouchDragImageVerticalOffset to the location, if it is not a tab
  // drag/drop.
  ui::GestureEvent touch_offset_event(*event,
                                      static_cast<aura::Window*>(nullptr),
                                      static_cast<aura::Window*>(nullptr));
  if (!toplevel_window_drag_delegate_) {
    gfx::PointF touch_offset_location = touch_offset_event.location_f();
    gfx::PointF touch_offset_root_location =
        touch_offset_event.root_location_f();
    touch_offset_location.Offset(0, kTouchDragImageVerticalOffset);
    touch_offset_root_location.Offset(0, kTouchDragImageVerticalOffset);
    touch_offset_event.set_location_f(touch_offset_location);
    touch_offset_event.set_root_location_f(touch_offset_root_location);
  }

  aura::Window* translated_target;
  if (capture_delegate_) {
    translated_target = capture_delegate_->GetTarget(touch_offset_event);
  } else {
    ui::Event::DispatcherApi(&touch_offset_event).set_target(event->target());
    translated_target =
        window_util::GetEventHandlerForEvent(touch_offset_event);
  }

  if (!translated_target) {
    DragCancel();
    event->StopPropagation();
    return;
  }

  std::unique_ptr<ui::LocatedEvent> translated_event;
  if (capture_delegate_) {
    translated_event =
        capture_delegate_->ConvertEvent(translated_target, touch_offset_event);
    DCHECK(translated_event);
  } else {
    translated_event = ConvertEvent(translated_target, touch_offset_event);
  }

  switch (event->type()) {
    case ui::EventType::kGestureScrollUpdate:
      DragUpdate(translated_target, *translated_event);
      break;
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kScrollFlingStart:
      Drop(translated_target, *translated_event);
      break;
    case ui::EventType::kGestureLongTap:
      // Ideally we would want to just forward this long tap event to the
      // |drag_source_window_|. However, webkit does not accept events while a
      // drag drop is still in progress. The drag drop ends only when the nested
      // message loop ends. Due to this, we have to defer forwarding
      // the long tap.
      if (capture_delegate_) {
        auto* capture_window =
            static_cast<aura::Window*>(capture_delegate_->capture_window());
        CHECK(capture_window);
        pending_long_tap_ = std::make_unique<ui::GestureEvent>(
            *event, capture_window,
            static_cast<aura::Window*>(drag_source_window_));
      } else {
        pending_long_tap_ = event->Clone();
      }
      DoDragCancel(kTouchCancelAnimationDuration);
      break;
    default:
      break;
  }

  if (toplevel_window_drag_delegate_)
    toplevel_window_drag_delegate_->OnToplevelWindowDragEvent(event);

  event->StopPropagation();
}

void DragDropController::OnWindowDestroying(aura::Window* window) {
  if (drag_window_ == window) {
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(drag_window_);
    if (delegate)
      delegate->OnDragExited();
    drag_window_->RemoveObserver(this);
    drag_window_ = nullptr;
  }
  if (drag_source_window_ == window) {
    if (drag_source_window_->HasObserver(this))
      drag_source_window_->RemoveObserver(this);
    drag_source_window_ = nullptr;

    // TabDragDropDelegate dereferences |drag_source_window_| in its logic,
    // and is meaningless without a valid instance of it.
    tab_drag_drop_delegate_.reset();
  }
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
  ui::DropTargetEvent e(*drag_data_.get(), event.location_f(),
                        event.root_location_f(), allowed_operations_);
  e.SetFlags(event.flags());
  ui::Event::DispatcherApi(&e).set_target(target);

  aura::client::DragUpdateInfo drag_info;
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
    if (delegate)
      delegate->OnDragEntered(e);
  } else {
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(drag_window_);
    if (delegate) {
      drag_info = delegate->OnDragUpdated(e);
      gfx::NativeCursor cursor = ui::mojom::CursorType::kNoDrop;
      if (drag_info.drag_operation & ui::DragDropTypes::DRAG_COPY)
        cursor = ui::mojom::CursorType::kCopy;
      else if (drag_info.drag_operation & ui::DragDropTypes::DRAG_LINK)
        cursor = ui::mojom::CursorType::kAlias;
      else if (drag_info.drag_operation & ui::DragDropTypes::DRAG_MOVE)
        cursor = ui::mojom::CursorType::kGrabbing;

      Shell::Get()->cursor_manager()->SetCursor(cursor);
    }
  }

  for (aura::client::DragDropClientObserver& observer : observers_)
    observer.OnDragUpdated(e);

  if (drag_info.drag_operation != current_drag_info_.drag_operation) {
    for (aura::client::DragDropClientObserver& observer : observers_)
      observer.OnDragActionsChanged(drag_info.drag_operation);
  }
  current_drag_info_ = drag_info;

  gfx::Point root_location_in_screen = event.root_location();
  ::wm::ConvertPointToScreen(target->GetRootWindow(), &root_location_in_screen);
  current_location_ = root_location_in_screen;

  if (drag_image_widget_) {
    DragImageView* drag_image =
        static_cast<DragImageView*>(drag_image_widget_->GetContentsView());
    drag_image->SetScreenPosition(root_location_in_screen - drag_image_offset_);
    drag_image->SetTouchDragOperation(drag_info.drag_operation);
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
  // We must guarantee that a target gets a OnDragEntered before Drop. WebKit
  // depends on not getting a Drop without DragEnter. This behavior is
  // consistent with drag/drop on other platforms.
  if (target != drag_window_)
    DragUpdate(target, event);
  DCHECK(target == drag_window_);

  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kPointer);

  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(target);

  aura::client::DragDropDelegate::DropCallback delegate_drop_cb =
      base::NullCallback();

  ui::DropTargetEvent e(*drag_data_.get(), event.location_f(),
                        event.root_location_f(), allowed_operations_);
  e.SetFlags(event.flags());
  ui::Event::DispatcherApi(&e).set_target(target);

  for (aura::client::DragDropClientObserver& observer : observers_) {
    observer.OnDragCompleted(e);
  }

  if (delegate) {
    delegate_drop_cb = delegate->GetDropCallback(e);
  }

  base::ScopedClosureRunner drag_cancel(base::BindOnce(
      &DragDropController::DragCancel, weak_factory_.GetWeakPtr()));

  gfx::Point drop_location_in_screen = event.root_location();
  ::wm::ConvertPointToScreen(target->GetRootWindow(), &drop_location_in_screen);

  const bool is_tab_drag_drop = (tab_drag_drop_delegate_.get() != nullptr);

  DCHECK_EQ(drag_window_, target);

  DropIfAllowed(
      drag_data_.get(), current_drag_info_,
      base::BindOnce(&DragDropController::PerformDrop,
                     weak_factory_.GetWeakPtr(), drop_location_in_screen, e,
                     std::move(drag_data_), std::move(delegate_drop_cb),
                     std::move(tab_drag_drop_delegate_),
                     std::move(drag_cancel)));

  Cleanup();

  // Tab drag-n-drop should never be async.
  if (is_tab_drag_drop)
    DCHECK(!drag_image_widget_);

  // If the drop is async and cancelled animation isn't running, reset
  // |drag_image_widget_|.
  if (!cancel_animation_)
    drag_image_widget_.reset();

  if (quit_closure_)
    std::move(quit_closure_).Run();
}

////////////////////////////////////////////////////////////////////////////////
// DragDropController, private:

void DragDropController::AnimationEnded(const gfx::Animation* animation) {
  cancel_animation_.reset();
  cancel_animation_notifier_.reset();

  drag_image_widget_.reset();
  ScheduleForwardPendingLongTap();
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

  if (toplevel_window_drag_delegate_)
    toplevel_window_drag_delegate_->OnToplevelWindowDragCancelled();

  for (aura::client::DragDropClientObserver& observer : observers_) {
    observer.OnDragCancelled();
  }
  Cleanup();

  StartCanceledAnimation(drag_cancel_animation_duration);

  if (quit_closure_)
    std::move(quit_closure_).Run();
}

void DragDropController::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK(drag_image_widget_);

  gfx::Rect current_bounds = animation->CurrentValueBetween(
      drag_image_initial_bounds_for_cancel_animation_,
      drag_image_final_bounds_for_cancel_animation_);
  static_cast<DragImageView*>(drag_image_widget_->GetContentsView())
      ->SetBoundsInScreen(current_bounds);
}

void DragDropController::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void DragDropController::OnWillApplyDisplayChanges() {
  // Abort in-progress drags if a monitor is added or removed because the drag
  // image widget's container may be destroyed.
  if (IsDragDropInProgress())
    DragCancel();
}

void DragDropController::StartCanceledAnimation(
    base::TimeDelta animation_duration) {
  DCHECK(!cancel_animation_);
  DCHECK(!will_forward_long_tap_);

  if (pending_long_tap_)
    will_forward_long_tap_ = true;

  if (!drag_image_widget_) {
    ScheduleForwardPendingLongTap();
    return;
  }

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

void DragDropController::ScheduleForwardPendingLongTap() {
  if (!pending_long_tap_)
    return;

  // If not in a nested run loop, we can forward the long tap right now.
  if (nested_loop_disabled_for_testing_) {
    ForwardPendingLongTap();
    return;
  }

  // See comment about this in OnGestureEvent().
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DragDropController::ForwardPendingLongTap,
                                weak_factory_.GetWeakPtr()));
}

void DragDropController::ForwardPendingLongTap() {
  if (drag_source_window_ && drag_source_window_->delegate()) {
    drag_source_window_->delegate()->OnGestureEvent(
        pending_long_tap_->AsGestureEvent());
    DispatchGestureEndToWindow(drag_source_window_);
  }

  CleanupPendingLongTap();
}

void DragDropController::Cleanup() {
  if (!capture_delegate_ && drag_source_window_ &&
      current_drag_event_source_ == ui::mojom::DragEventSource::kMouse) {
    drag_source_window_->ReleaseCapture();
  }

  // Do not remove observer if `drag_window_` is the same as
  // `drag_source_window_`.
  // `drag_source_window_` is still necessary to process long tab and the
  // observer will be reset when `drag_source_window_` is destroyed.
  if (drag_window_ && drag_window_ != drag_source_window_)
    drag_window_->RemoveObserver(this);
  drag_window_ = nullptr;
  drag_drop_completed_ = true;
  drag_data_.reset();
  allowed_operations_ = 0;
  tab_drag_drop_delegate_.reset();
  touch_drag_drop_delegate_.reset();
  capture_delegate_ = nullptr;
}

void DragDropController::CleanupPendingLongTap() {
  pending_long_tap_.reset();
  will_forward_long_tap_ = false;
  if (drag_source_window_)
    drag_source_window_->RemoveObserver(this);
  drag_source_window_ = nullptr;
}

void DragDropController::PerformDrop(
    const gfx::Point drop_location_in_screen,
    ui::DropTargetEvent event,
    std::unique_ptr<ui::OSExchangeData> drag_data,
    aura::client::DragDropDelegate::DropCallback drop_cb,
    std::unique_ptr<TabDragDropDelegate> tab_drag_drop_delegate,
    base::ScopedClosureRunner cancel_drag_callback) {
  // Event copy constructor dooesn't copy the target. That's why we set it here.
  // DragDropController observes the `drag_window_`, so if it's destroyed, the
  // target will be set to nullptr.
  ui::Event::DispatcherApi(&event).set_target(drag_window_);

  ui::OSExchangeData copied_data(drag_data->provider().Clone());
  if (!!drop_cb) {
    std::move(drop_cb).Run(
        std::move(drag_data), operation_,
        drag_image_widget_
            ? ::wm::RecreateLayers(drag_image_widget_->GetNativeWindow())
            : nullptr);
  }

  if (operation_ == DragOperation::kNone && tab_drag_drop_delegate) {
    // Release the ownership of object so that it can delete itself.
    tab_drag_drop_delegate.release()->DropAndDeleteSelf(drop_location_in_screen,
                                                        copied_data);
    // Override the drag event's drop effect as a move to inform the front-end
    // that the tab or group was moved. Otherwise, the WebUI tab strip does
    // not know that a drop resulted in a tab being moved and will temporarily
    // visually return the tab to its original position. (crbug.com/1081905)
    operation_ = DragOperation::kMove;
    drag_image_widget_.reset();
  } else if (operation_ == DragOperation::kNone) {
    StartCanceledAnimation(kCancelAnimationDuration);
  } else {
    drag_image_widget_.reset();
  }

  if (toplevel_window_drag_delegate_) {
    operation_ = toplevel_window_drag_delegate_->OnToplevelWindowDragDropped();
  }

  for (aura::client::DragDropClientObserver& observer : observers_) {
    observer.OnDropCompleted(operation_);
  }

  // Drop completed, so no need to cancel the drop.
  std::ignore = cancel_drag_callback.Release();
}

void DragDropController::CancelIfInProgress() {
  if (IsDragDropInProgress())
    DragCancel();
}

}  // namespace ash
