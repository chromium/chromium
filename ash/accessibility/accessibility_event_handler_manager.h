// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_EVENT_HANDLER_MANAGER_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_EVENT_HANDLER_MANAGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"

namespace ash {

// Manages ordering of the Accessibility event handlers so that the handlers
// with the highest priority get events before the others. For example,
// the cursor must always get events before magnifier so that it can be drawn,
// and magnifier must always get events before Select to Speak so that mouse
// moves cause the viewport to move.
class AccessibilityEventHandlerManager {
 public:
  // Ordered by priority. Earlier enums will get events before later ones.
  // Create a new level for each type of accessibility EventHandler.
  enum class HandlerType {
    kMouseKeys = 0,
    kCursor,
    kFullscreenMagnifier,
    kDockedMagnifier,
    kChromeVox,
    kAutoclick,
    kSelectToSpeak,
    kMaxValue = kSelectToSpeak,
  };

  AccessibilityEventHandlerManager();
  AccessibilityEventHandlerManager(const AccessibilityEventHandlerManager&) =
      delete;
  AccessibilityEventHandlerManager& operator=(
      const AccessibilityEventHandlerManager&) = delete;
  ~AccessibilityEventHandlerManager();

  void AddAccessibilityEventHandler(
      ui::EventHandler* handler,
      AccessibilityEventHandlerManager::HandlerType type);
  void RemoveAccessibilityEventHandler(ui::EventHandler* handler);

 private:
  void UpdateEventHandlers();

  // List of the current event handlers, indexed by
  // AccessibilityEventHandlerType.
  std::vector<raw_ptr<ui::EventHandler, VectorExperimental>> event_handlers_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_EVENT_HANDLER_MANAGER_H_
