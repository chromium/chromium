// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_MODEL_QUICK_INSERT_CAPS_LOCK_POSITION_H_
#define ASH_QUICK_INSERT_MODEL_QUICK_INSERT_CAPS_LOCK_POSITION_H_

namespace ash {

enum class PickerCapsLockPosition {
  // First item in the zero-state view.
  kTop,
  // Last item in the suggested section in the zero-state view (best effort).
  kMiddle,
  // Last item in the zero-state view.
  kBottom,
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_MODEL_QUICK_INSERT_CAPS_LOCK_POSITION_H_
