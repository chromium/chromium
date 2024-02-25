// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_KEY_EVENT_HANDLER_H_
#define ASH_PICKER_VIEWS_PICKER_KEY_EVENT_HANDLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class KeyEvent;
}

namespace ash {

class PickerKeyEventTarget;

// Helper for routing and handling key events, e.g. for keyboard navigation.
class ASH_EXPORT PickerKeyEventHandler {
 public:
  PickerKeyEventHandler();
  PickerKeyEventHandler(const PickerKeyEventHandler&) = delete;
  PickerKeyEventHandler& operator=(const PickerKeyEventHandler&) = delete;
  ~PickerKeyEventHandler();

  // Returns true if the key event was handled and should not be further
  // processed.
  bool HandleKeyEvent(const ui::KeyEvent& event);

  void SetActiveKeyEventTarget(PickerKeyEventTarget* active_key_event_target);

 private:
  raw_ptr<PickerKeyEventTarget> active_key_event_target_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_KEY_EVENT_HANDLER_H_
