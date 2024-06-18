// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_
#define ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/drag_drop/drag_drop_capture_delegate.h"
#include "ash/drag_drop/tab_drag_drop_delegate.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class LinearAnimation;
}

namespace ui {
class LocatedEvent;
}

namespace ash {
class ToplevelWindowDragDelegate;

class ASH_EXPORT DragDropController : public aura::client::DragDropClient,
                                      public ui::EventHandler,
                                      public gfx::AnimationDelegate,
                                      public aura::WindowObserver,
                                      public display::DisplayManagerObserver {
 public:
  DragDropController();

  DragDropController(const DragDropController&) = delete;
  DragDropController& operator=(const DragDropController&) = delete;

  ~DragDropController() override;

  void set_enabled(bool enabled) { enabled_ = enabled; }

  void set_toplevel_window_drag_delegate(ToplevelWindowDragDelegate* delegate) {
    toplevel_window_drag_delegate_ = delegate;
  }

  // Returns if the drag drop operation has been fully completed.  This is
  // similar to IsDragDropInProgress, but returns true even after the drop_data
  // is passed to the target, and keep returning true until the drag drop states
  // are callbacks are called), so that the callback receive the proper
  // state.
  bool IsDragDropCompleted();

  // Overridden from aura::client::DragDropClient:
  ui::mojom::DragOperation StartDragAndDrop(
      std::unique_ptr<ui::OSExchangeData> data,
      aura::Window* root_window,
      aura::Window* source_window,
      const gfx::Point& screen_location,
      int allowed_operations,
      ui::mojom::DragEventSource source) override;
  void DragCancel() override;
  bool IsDragDropInProgress() override;
  void AddObserver(aura::client::DragDropClientObserver* observer) override;
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from aura::WindowObserver.
  void OnWindowDestroying(aura::Window* window) override;

  void SetDragImage(const gfx::ImageSkia& image,
                    const gfx::Vector2d& image_offset);

  ui::mojom::DragEventSource event_source() {
    return current_drag_event_source_;
  }

  // Sets the `closure` that will be executed as a replacement of
  // inner event loop. A test can use this closure to generate events, or
  // take other actions that should happen during the drag and drop, and
  // can also check the condition that should be satisfied.
  // The loop closure is called with a boolean value that indicates
  // that this is called from the inner loop because the same closure will
  // often used to generate the event that will eventually enter the drag
  // and drop inner loop. The `quit_closure` is used for a test
  // to exit the outer loop in the test.
  using TestLoopClosure = base::RepeatingCallback<void()>;
  void SetLoopClosureForTesting(TestLoopClosure closure,
                                base::OnceClosure quit_closure);

  void SetDisableNestedLoopForTesting(bool disable);

  // Deprecated: Use `SetDisableNestedLoopForTesting`.
  void set_should_block_during_drag_drop(bool should_block_during_drag_drop) {
    SetDisableNestedLoopForTesting(!should_block_during_drag_drop);
  }

  void enable_no_image_touch_drag_for_test() {
    allow_no_image_touch_drag_for_test_ = true;
  }

 protected:
  // Helper method to create a LinearAnimation object that will run the drag
  // cancel animation. Caller take ownership of the returned object. Protected
  // for testing.
  virtual gfx::LinearAnimation* CreateCancelAnimation(
      base::TimeDelta duration,
      int frame_rate,
      gfx::AnimationDelegate* delegate);

  // Exposed for tests to override.
  virtual void DragUpdate(aura::Window* target, const ui::LocatedEvent& event);
  virtual void Drop(aura::Window* target, const ui::LocatedEvent& event);

  // Actual implementation of |DragCancel()|. protected for testing.
  virtual void DoDragCancel(base::TimeDelta drag_cancel_animation_duration);

  // Exposed for test assertions.
  DragDropCaptureDelegate* get_capture_delegate() { return capture_delegate_; }

 private:
  friend class DragDropControllerTest;
  friend class DragDropControllerTestApi;

  // Overridden from gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // display::DisplayManagerObserver
  void OnWillApplyDisplayChanges() override;

  // Helper method to start drag widget flying back animation.
  void StartCanceledAnimation(base::TimeDelta animation_duration);

  // Helper methods to forward |pending_log_tap_| event to
  // |drag_source_window_|.
  void ScheduleForwardPendingLongTap();
  void ForwardPendingLongTap();

  // Helper method to reset most of the state, except state that could be used
  // during async operations of cancellation (including cancel animation and
  // posting task to dispatch long tap event).
  void Cleanup();

  void CleanupPendingLongTap();

  // Performs data drop. NOTE: this method does not run in an async drop if
  // disallowed by `ui::DataTransferPolicyController`. `cancel_drag_callback`
  // runs if this method does not run.
  void PerformDrop(const gfx::Point drop_location_in_screen,
                   ui::DropTargetEvent event,
                   std::unique_ptr<ui::OSExchangeData> drag_data,
                   aura::client::DragDropDelegate::DropCallback drop_cb,
                   std::unique_ptr<TabDragDropDelegate> tab_drag_drop_delegate,
                   base::ScopedClosureRunner cancel_drag_callback);

  void CancelIfInProgress();

  bool enabled_ = false;
  bool drag_drop_completed_ = true;
  std::unique_ptr<views::Widget> drag_image_widget_;
  gfx::Vector2d drag_image_offset_;
  std::unique_ptr<ui::OSExchangeData> drag_data_;
  int allowed_operations_ = 0;
  ui::mojom::DragOperation operation_ = ui::mojom::DragOperation::kNone;
  aura::client::DragUpdateInfo current_drag_info_;

  // Used when processing a Chrome tab drag from a WebUI tab strip.
  std::unique_ptr<TabDragDropDelegate> tab_drag_drop_delegate_;

  // Used when processing a normal drag and drop with touch.
  std::unique_ptr<DragDropCaptureDelegate> touch_drag_drop_delegate_;

  // Window that is currently under the drag cursor.
  raw_ptr<aura::Window> drag_window_ = nullptr;

  // Starting and final bounds for the drag image for the drag cancel animation.
  gfx::Rect drag_image_initial_bounds_for_cancel_animation_;
  gfx::Rect drag_image_final_bounds_for_cancel_animation_;

  std::unique_ptr<gfx::LinearAnimation> cancel_animation_;
  std::unique_ptr<gfx::AnimationDelegate> cancel_animation_notifier_;

  // Window that started the drag.
  raw_ptr<aura::Window> drag_source_window_ = nullptr;

  // A closure that allows a test to implement the actions within
  // drag and drop event loop.
  TestLoopClosure test_loop_closure_;

  // True if the nested event loop is disabled.
  bool nested_loop_disabled_for_testing_ = false;

  // Closure for quitting nested run loop.
  base::OnceClosure quit_closure_;

  // If non-null, a drag is active which required a capture window.
  raw_ptr<DragDropCaptureDelegate, DanglingUntriaged> capture_delegate_ =
      nullptr;

  ui::mojom::DragEventSource current_drag_event_source_ =
      ui::mojom::DragEventSource::kMouse;

  // Holds a synthetic long tap event to be sent to the |drag_source_window_|.
  // See comment in OnGestureEvent() on why we need this.
  std::unique_ptr<ui::Event> pending_long_tap_;
  // Set to true during async operations of cancellation (including cancel
  // animation and posting task to dispatch long tap event), indicating that a
  // long tap event will be dispatched.
  bool will_forward_long_tap_ = false;

  gfx::Point start_location_;
  gfx::Point current_location_;

  base::ObserverList<aura::client::DragDropClientObserver>::
      UncheckedAndDanglingUntriaged observers_;

  raw_ptr<ToplevelWindowDragDelegate, DanglingUntriaged>
      toplevel_window_drag_delegate_ = nullptr;

  bool allow_no_image_touch_drag_for_test_ = false;

  // Weak ptr for async callbacks to be invalidated if a new drag starts.
  base::WeakPtrFactory<DragDropController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_
