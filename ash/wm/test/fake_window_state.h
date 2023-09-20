// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TEST_FAKE_WINDOW_STATE_H_
#define ASH_WM_TEST_FAKE_WINDOW_STATE_H_

#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "chromeos/ui/base/window_state_type.h"

namespace ash {

// WindowState based on a given initial state. Records things such as if the
// window was visible on minimize event, number of system UI area changes, and
// last requested bounds.
class FakeWindowState : public WindowState::State {
 public:
  explicit FakeWindowState(chromeos::WindowStateType initial_state_type);
  FakeWindowState(const FakeWindowState&) = delete;
  FakeWindowState& operator=(const FakeWindowState&) = delete;
  ~FakeWindowState() override;

  bool was_visible_on_minimize() const { return was_visible_on_minimize_; }

  const gfx::Rect& last_requested_bounds() const {
    return last_requested_bounds_;
  }

  // WindowState::State:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override;
  chromeos::WindowStateType GetType() const override;
  void AttachState(WindowState* window_state,
                   WindowState::State* previous_state) override {}
  void DetachState(WindowState* window_state) override {}

 private:
  bool was_visible_on_minimize_ = true;
  gfx::Rect last_requested_bounds_;
  const chromeos::WindowStateType state_type_;
};

class FakeWindowStateDelegate : public WindowStateDelegate {
 public:
  FakeWindowStateDelegate();
  FakeWindowStateDelegate(const FakeWindowStateDelegate&) = delete;
  FakeWindowStateDelegate& operator=(const FakeWindowStateDelegate&) = delete;
  ~FakeWindowStateDelegate() override;

  int toggle_locked_fullscreen_count() const {
    return toggle_locked_fullscreen_count_;
  }

  bool drag_in_progress() const { return drag_in_progress_; }
  int drag_start_component() const { return drag_start_component_; }
  gfx::PointF drag_end_location() const { return drag_end_location_; }

  // WindowStateDelegate:
  bool ToggleFullscreen(WindowState* window_state) override;
  void ToggleLockedFullscreen(WindowState* window_state) override;
  std::unique_ptr<PresentationTimeRecorder> OnDragStarted(
      int component) override;
  void OnDragFinished(bool cancel, const gfx::PointF& location) override;

 private:
  int toggle_locked_fullscreen_count_ = 0;
  bool drag_in_progress_ = false;
  int drag_start_component_ = -1;
  gfx::PointF drag_end_location_;
};

}  // namespace ash

#endif  // ASH_WM_TEST_FAKE_WINDOW_STATE_H_
