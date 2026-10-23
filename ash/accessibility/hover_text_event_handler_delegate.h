// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_HOVER_TEXT_EVENT_HANDLER_DELEGATE_H_
#define ASH_ACCESSIBILITY_HOVER_TEXT_EVENT_HANDLER_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"

namespace gfx {
class Point;
}

namespace ash {

// A delegate for HoverTextEventHandler to communicate with
// AccessibilityController or other test clients.
class ASH_EXPORT HoverTextEventHandlerDelegate {
 public:
  virtual ~HoverTextEventHandlerDelegate() = default;

  // Called when the Hover Text activation state changes via key press, key
  // release, or tray toggle. If stop_speech is true, signals to the
  // extension to immediately halt active TTS.
  virtual void OnHoverTextKeyEvent(bool is_active, bool stop_speech) = 0;

  // Forwards screen coordinates to the AccessibilityController to update
  // the position of the Hover Text bubble.
  virtual void OnHoverTextPointUpdated(const gfx::Point& point_in_screen,
                                       HoverTextPointerType pointer_type) = 0;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_HOVER_TEXT_EVENT_HANDLER_DELEGATE_H_
