// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_MODEL_PICKER_ACTION_TYPE_H_
#define ASH_PICKER_MODEL_PICKER_ACTION_TYPE_H_

#include "ash/ash_export.h"

namespace ash {

enum class PickerActionType {
  // Performs the action represented by the result.
  kDo,
  // Inserts the result into the focused input field.
  kInsert,
  // Opens the result in some window.
  kOpen,
  // Requests the result to be created.
  kCreate,
};

}

#endif  // ASH_PICKER_MODEL_PICKER_ACTION_TYPE_H_
