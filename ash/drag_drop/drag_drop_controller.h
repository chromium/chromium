// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_
#define ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window_observer.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class LinearAnimation;
}

namespace ui {
class LocatedEvent;
}

namespace ash {
class DragDropTracker;
class DragDropTrackerDelegate;
class DragImageView;

class ASH_EXPORT DragDropController : public aura::client::DragDropClient,
                                      public ui::EventHandler,
                                      public gfx::AnimationDelegate,
                                      public aura::WindowObserver,
                                      public WindowTreeHostManager::Observer {
 public:
  DragDropController();
  ~DragDropController() override;

  void set_should_block_during_drag_drop(bool should_block_during_drag_drop) {
    should_block_during_drag_drop_ = should_block_during_drag_drop;
  }

  void set_enabled(bool enabled) { enabled_ = enabled; }

  // Overridden from aura::client::DragDropClient:
  int StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& screen_location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override;
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
  void OnWindowDestroyed(aura::Window* window) override;

  void SetDragImage(const gfx::ImageSkia& image,
                    const gfx::Vector2d& image_offset);

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

 private:
  friend class DragDropControllerTest;
  friend class DragDropControllerTestApi;

  // Overridden from gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanging() override;

  // Helper method to start drag widget flying back animation.
  void StartCanceledAnimation(base::TimeDelta animation_duration);

  // Helper method to forward |pending_log_tap_| event to |drag_source_window_|.
  void ForwardPendingLongTap();

  // Helper method to reset everything.
  void Cleanup();

  bool enabled_ = false;
  std::unique_ptr<DragImageView> drag_image_;
  gfx::Vector2d drag_image_offset_;
  std::unique_ptr<ui::OSExchangeData> drag_data_;
  int drag_operation_;
  int current_drag_actions_ = 0;

  // Window that is currently under the drag cursor.
  aura::Window* drag_window_;

  // Starting and final bounds for the drag image for the drag cancel animation.
  gfx::Rect drag_image_initial_bounds_for_cancel_animation_;
  gfx::Rect drag_image_final_bounds_for_cancel_animation_;

  std::unique_ptr<gfx::LinearAnimation> cancel_animation_;

  // Window that started the drag.
  aura::Window* drag_source_window_;

  // Indicates whether the caller should be blocked on a drag/drop session.
  // Only be used for tests.
  bool should_block_during_drag_drop_;

  // Closure for quitting nested run loop.
  base::Closure quit_closure_;

  std::unique_ptr<ash::DragDropTracker> drag_drop_tracker_;
  std::unique_ptr<DragDropTrackerDelegate> drag_drop_window_delegate_;

  ui::DragDropTypes::DragEventSource current_drag_event_source_;

  // Holds a synthetic long tap event to be sent to the |drag_source_window_|.
  // See comment in OnGestureEvent() on why we need this.
  std::unique_ptr<ui::GestureEvent> pending_long_tap_;

  gfx::Point start_location_;
  gfx::Point current_location_;

  base::ObserverList<aura::client::DragDropClientObserver>::Unchecked
      observers_;

  base::WeakPtrFactory<DragDropController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DragDropController);
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_
