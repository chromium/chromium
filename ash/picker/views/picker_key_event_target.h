// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_KEY_EVENT_TARGET_H_
#define ASH_PICKER_VIEWS_PICKER_KEY_EVENT_TARGET_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT PickerKeyEventTarget {
 public:
  virtual ~PickerKeyEventTarget() = default;

  // Returns true if the enter key press was handled and should not be further
  // processed.
  virtual bool OnEnterKeyPressed() = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_KEY_EVENT_TARGET_H_
