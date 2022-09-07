// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_STATE_DELEGATE_H_
#define ASH_WM_WINDOW_STATE_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/presentation_time_recorder.h"

namespace gfx {
class PointF;
}

namespace ash {

class WindowState;

class ASH_EXPORT WindowStateDelegate {
 public:
  WindowStateDelegate();

  WindowStateDelegate(const WindowStateDelegate&) = delete;
  WindowStateDelegate& operator=(const WindowStateDelegate&) = delete;

  virtual ~WindowStateDelegate();

  // Toggles the window into or out of the fullscreen state. If the window is
  // not fullscreen and the window supports immersive fullscreen
  // ToggleFullscreen() should put the window into immersive fullscreen instead
  // of the default fullscreen type. The caller (ash::WindowState) falls backs
  // to the default implementation if this returns false.
  virtual bool ToggleFullscreen(WindowState* window_state);

  // Toggles the locked fullscreen state, aka Pinned and TrustedPinned, where a
  // window has exclusive control of the screen. Implementers should implement
  // restrictions related to the relevant pinned mode for their window in this
  // function.
  virtual void ToggleLockedFullscreen(WindowState* window_state);

  // Invoked when the user started drag operation. |component| must be
  // a member of ui::HitTestCompat enum and specifies which part of
  // the window the pointer device was on when the user started drag
  // operation. Returns a presentation time recorder that could be used to
  // track resize latency.
  virtual std::unique_ptr<PresentationTimeRecorder> OnDragStarted(
      int component);

  // Invoked when the user finished drag operation. |cancel| is true
  // if the drag operation was canceled.
  virtual void OnDragFinished(bool cancel, const gfx::PointF& location) {}
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_STATE_DELEGATE_H_
