// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FLOAT_SCOPED_WINDOW_TUCKER_H_
#define ASH_WM_FLOAT_SCOPED_WINDOW_TUCKER_H_

#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

// Scoped class which makes modifications while a window is tucked. It owns a
// tuck handle widget that will bring the hidden window back onscreen. Users of
// the class need to ensure that window outlives instance of this class.
class ScopedWindowTucker : public wm::ActivationChangeObserver {
 public:
  // Creates an instance for `window` where `left` is the side of the screen
  // that the tuck handle is on.
  ScopedWindowTucker(aura::Window* window, bool left);
  ScopedWindowTucker(const ScopedWindowTucker&) = delete;
  ScopedWindowTucker& operator=(const ScopedWindowTucker&) = delete;
  ~ScopedWindowTucker() override;

  views::Widget* tuck_handle_widget() { return tuck_handle_widget_.get(); }

  void AnimateTuck();

  // Runs `callback` when the animation is completed.
  void AnimateUntuck(base::OnceClosure callback);

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  class TuckHandle;

  // Destroys `this_`, which will untuck `window_` and set the window bounds
  // back onscreen.
  void UntuckWindow();

  // The window that is being tucked. Will be tucked and untucked by the tuck
  // handle.
  aura::Window* window_;

  // True iff the window is tucked to the left screen edge, false otherwise.
  bool left_ = false;

  views::UniqueWidgetPtr tuck_handle_widget_ =
      std::make_unique<views::Widget>();
};

}  // namespace ash

#endif  // ASH_WM_FLOAT_SCOPED_WINDOW_TUCKER_H_
