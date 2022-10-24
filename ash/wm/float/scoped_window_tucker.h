// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FLOAT_SCOPED_WINDOW_TUCKER_H_
#define ASH_WM_FLOAT_SCOPED_WINDOW_TUCKER_H_

#include "ash/ash_export.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// Scoped class which makes modifications while a window is tucked. It owns a
// tuck handle widget that will bring the hidden window back onscreen.
class ScopedWindowTucker {
 public:
  // Creates an instance for `window` where `left` is the side of the screen
  // that the tuck handle is on.
  ScopedWindowTucker(aura::Window* window, bool left);
  ScopedWindowTucker(const ScopedWindowTucker&) = delete;
  ScopedWindowTucker& operator=(const ScopedWindowTucker&) = delete;
  ~ScopedWindowTucker() = default;

  views::Widget* tuck_handle_widget() { return tuck_handle_widget_.get(); }

  void AnimateTuck(bool left);

  void OnButtonPressed();

 private:
  class TuckHandle;

  // The window that is being tucked. Will be tucked and untucked by the tuck
  // handle.
  aura::Window* window_;

  views::UniqueWidgetPtr tuck_handle_widget_ =
      std::make_unique<views::Widget>();
};

}  // namespace ash

#endif  // ASH_WM_FLOAT_SCOPED_WINDOW_TUCKER_H_