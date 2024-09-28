// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_API_ERROR_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_API_ERROR_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

namespace ash {

// Describes the last error received from one of the API backends.
// `error_message` is suitable for display to the user.
struct ASH_EXPORT FocusModeApiError {
  bool fatal = false;
  std::string error_message;
};

using ApiErrorCallback =
    base::RepeatingCallback<void(const FocusModeApiError& error)>;

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_API_ERROR_H_
