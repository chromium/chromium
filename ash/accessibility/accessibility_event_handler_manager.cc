// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_event_handler_manager.h"
#include "ash/shell.h"
#include "base/ranges/algorithm.h"
#include "ui/events/event_handler.h"

namespace ash {

AccessibilityEventHandlerManager::AccessibilityEventHandlerManager()
    : event_handlers_(static_cast<int>(HandlerType::kMaxValue) + 1, nullptr) {}

AccessibilityEventHandlerManager::~AccessibilityEventHandlerManager() = default;

void AccessibilityEventHandlerManager::AddAccessibilityEventHandler(
    ui::EventHandler* handler,
    HandlerType type) {
  DCHECK(handler);
  int index = static_cast<int>(type);
  if (event_handlers_[index] == handler)
    return;

  DCHECK_EQ(event_handlers_[index], nullptr)
      << "Multiple event handlers should not share a priority level.";
  event_handlers_[index] = handler;
  UpdateEventHandlers();
}

void AccessibilityEventHandlerManager::RemoveAccessibilityEventHandler(
    ui::EventHandler* handler) {
  DCHECK(handler);
  auto it = base::ranges::find(event_handlers_, handler);
  DCHECK(it != event_handlers_.end());

  // Remove it from our list.
  *it = nullptr;

  // No need to call UpdateEventHandlers as nothing is re-ordered.
  ash::Shell::Get()->RemovePreTargetHandler(handler);
}

void AccessibilityEventHandlerManager::UpdateEventHandlers() {
  // Remove them all and add them again so they are guaranteed to be in the
  // right order.
  for (ui::EventHandler* handler : event_handlers_) {
    if (handler == nullptr)
      continue;
    ash::Shell::Get()->RemovePreTargetHandler(handler);
  }
  // Add the event handlers in reverse order. Pre-target handlers are
  // pushed into the font of the EventHandler list, so by adding the
  // highest priority EventHandler last we ensure it's at the top.
  for (auto it = event_handlers_.rbegin(); it != event_handlers_.rend(); ++it) {
    if (*it == nullptr)
      continue;
    ash::Shell::Get()->AddPreTargetHandler(
        *it, ui::EventTarget::Priority::kAccessibility);
  }
}
}  // namespace ash
