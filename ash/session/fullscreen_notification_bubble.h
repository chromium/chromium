// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_FULLSCREEN_NOTIFICATION_BUBBLE_H_
#define ASH_SESSION_FULLSCREEN_NOTIFICATION_BUBBLE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

class SubtleNotificationView;

namespace base {
class OneShotTimer;
}

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {

// A notification bubble shown when the device returns from sleep, low
// brightness or the lock screen to remind the user that full screen mode is
// active.
class ASH_EXPORT FullscreenNotificationBubble : public aura::WindowObserver,
                                                public WindowStateObserver {
 public:
  FullscreenNotificationBubble();

  FullscreenNotificationBubble(const FullscreenNotificationBubble&) = delete;
  FullscreenNotificationBubble& operator=(const FullscreenNotificationBubble&) =
      delete;

  ~FullscreenNotificationBubble() override;

  void ShowForWindowState(WindowState* window_state);

  // Returns the underlying widget for testing purposes.
  views::Widget* widget_for_test() { return widget_; }

 private:
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // WindowStateObserver:
  void OnPreWindowStateTypeChange(WindowState* window_state,
                                  chromeos::WindowStateType old_type) override;

  void Show();
  void Hide();

  gfx::Rect GetBubbleBounds();

  // The contents of the widget.
  raw_ptr<SubtleNotificationView> view_ = nullptr;

  // The widget containing the bubble.
  raw_ptr<views::Widget> widget_ = nullptr;

  // A timer to auto-dismiss the bubble after a short period of time.
  std::unique_ptr<base::OneShotTimer> timer_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
  base::ScopedObservation<ash::WindowState, ash::WindowStateObserver>
      window_state_observation_{this};

  base::WeakPtrFactory<FullscreenNotificationBubble> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SESSION_FULLSCREEN_NOTIFICATION_BUBBLE_H_
