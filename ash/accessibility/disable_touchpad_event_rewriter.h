// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_DISABLE_TOUCHPAD_EVENT_REWRITER_H_
#define ASH_ACCESSIBILITY_DISABLE_TOUCHPAD_EVENT_REWRITER_H_

#include "ash/ash_export.h"
#include "ui/events/event_rewriter.h"

namespace ash {

// EventRewriter that cancels events from the built-in touchpad.
class ASH_EXPORT DisableTouchpadEventRewriter : public ui::EventRewriter {
 public:
  DisableTouchpadEventRewriter();
  DisableTouchpadEventRewriter(const DisableTouchpadEventRewriter&) = delete;
  DisableTouchpadEventRewriter& operator=(const DisableTouchpadEventRewriter&) =
      delete;
  ~DisableTouchpadEventRewriter() override;

  void SetEnabled(bool enabled);
  bool IsEnabled();

 private:
  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  void HandleKeyEvent(const ui::KeyEvent* event);
  void HandleShiftKeyPress();
  void ResetShiftKeyPressTracking();
  ui::EventDispatchDetails HandleMouseOrScrollEvent(
      const ui::Event& event,
      const Continuation continuation);

  bool enabled_ = false;
  int shift_press_count_ = 0;
  base::TimeTicks first_shift_press_time_ = base::TimeTicks();
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_DISABLE_TOUCHPAD_EVENT_REWRITER_H_
