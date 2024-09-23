// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_UTILS_H_
#define ASH_SYSTEM_MAHI_MAHI_UTILS_H_

#include "ash/ash_export.h"

namespace chromeos {
enum class MahiResponseStatus;
}  // namespace chromeos

namespace ash::mahi_utils {

// Returns the retry link's target visible for `error`.
// NOTE: This function should be called only if the `error` should be presented
// on `MahiErrorStatusView`.
ASH_EXPORT bool CalculateRetryLinkVisible(chromeos::MahiResponseStatus error);

// Returns the text ID of the `error` description on `MahiErrorStatusView`.
// NOTE: This function should be called only if the `error` should be presented
// on `MahiErrorStatusView`.
ASH_EXPORT int GetErrorStatusViewTextId(chromeos::MahiResponseStatus error);

}  // namespace ash::mahi_utils

#endif  // ASH_SYSTEM_MAHI_MAHI_UTILS_H_
