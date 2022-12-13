// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_

#include "base/timer/timer.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ui {
class KeyEvent;
}  // namespace ui

namespace ash {

class KeyComboView;
class VideoRecordingWatcher;

// Observes and decides whether to show a helper widget representing the
// currently pressed key combination or not. The key combination will be used to
// construct or modify the `KeyComboViewer`. The
// `CaptureModeDemoToolsController` will only be available during video
// recording and has to be explicitly enabled by the user.
class CaptureModeDemoToolsController {
 public:
  explicit CaptureModeDemoToolsController(
      VideoRecordingWatcher* video_recording_watcher);
  CaptureModeDemoToolsController(const CaptureModeDemoToolsController&) =
      delete;
  CaptureModeDemoToolsController& operator=(
      const CaptureModeDemoToolsController&) = delete;
  ~CaptureModeDemoToolsController();

  // Decides whether to show a helper widget for the `event` or not.
  void OnKeyEvent(ui::KeyEvent* event);

 private:
  friend class CaptureModeDemoToolsTestApi;

  void OnKeyUpEvent(ui::KeyEvent* event);
  void OnKeyDownEvent(ui::KeyEvent* event);

  // Refreshes the state of the `key_combo_view_` based on the current state of
  // `modifiers_` and `last_non_modifier_key_`.
  void RefreshKeyComboViewer();

  gfx::Rect CalculateBounds() const;

  // Resets the `demo_tools_widget_` when the `hide_timer_` expires.
  void AnimateToResetTheWidget();

  VideoRecordingWatcher* const video_recording_watcher_;
  views::UniqueWidgetPtr demo_tools_widget_;
  KeyComboView* key_combo_view_ = nullptr;

  // The state of the modifier keys i.e. Shift/Ctrl/Alt/Launcher keys.
  int modifiers_ = 0;

  // The most recently pressed non-modifier key.
  ui::KeyboardCode last_non_modifier_key_ = ui::VKEY_UNKNOWN;

  // Starts on key up of the last non-modifier key and the `key_combo_view_`
  // will disappear when it expires.
  base::OneShotTimer hide_timer_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_