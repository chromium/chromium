// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_OBSERVER_H_
#define ASH_SYSTEM_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT VirtualKeyboardObserver {
 public:
  virtual ~VirtualKeyboardObserver() {}

  // Notifies when the keyboard is suppressed.
  virtual void OnKeyboardSuppressionChanged(bool suppressed) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_OBSERVER_H_
