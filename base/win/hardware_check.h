// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_HARDWARE_CHECK_H_
#define BASE_WIN_HARDWARE_CHECK_H_

#include "base/base_export.h"

namespace base::win {

// Returns true if the hardware supports Win11. It is intended to be called
// on OS versions below Win11 and validates against minimum requirements.
// This must be called from a context that allows I/O operations.
BASE_EXPORT bool IsWin11UpgradeEligible();

}  // namespace base::win

#endif  // BASE_WIN_HARDWARE_CHECK_H_
