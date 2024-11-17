// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_api_error.h"

namespace ash {

bool FocusModeApiError::IsFatal() const {
  return type != Type::kOther;
}

}  // namespace ash
