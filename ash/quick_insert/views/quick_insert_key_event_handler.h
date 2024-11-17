// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_KEY_EVENT_HANDLER_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_KEY_EVENT_HANDLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class KeyEvent;
}

namespace ash {

class QuickInsertPseudoFocusHandler;

// Helper for routing and handling key events, e.g. for keyboard navigation.
class ASH_EXPORT QuickInsertKeyEventHandler {
 public:
  QuickInsertKeyEventHandler();
  QuickInsertKeyEventHandler(const QuickInsertKeyEventHandler&) = delete;
  QuickInsertKeyEventHandler& operator=(const QuickInsertKeyEventHandler&) =
      delete;
  ~QuickInsertKeyEventHandler();

  // Returns true if the key event was handled and should not be further
  // processed.
  bool HandleKeyEvent(const ui::KeyEvent& event);

  void SetActivePseudoFocusHandler(
      QuickInsertPseudoFocusHandler* active_pseudo_focus_handler);

 private:
  raw_ptr<QuickInsertPseudoFocusHandler> active_pseudo_focus_handler_ = nullptr;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_KEY_EVENT_HANDLER_H_
