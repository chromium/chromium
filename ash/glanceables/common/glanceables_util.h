// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_GLANCEABLES_UTIL_H_
#define ASH_GLANCEABLES_COMMON_GLANCEABLES_UTIL_H_

#include "ash/ash_export.h"

namespace ash::glanceables_util {

// Checks if the default network is connected or not.
bool IsNetworkConnected();

// Manually sets the network connected state used `IsNetworkConnected()` in
// for testing.
ASH_EXPORT void SetIsNetworkConnectedForTest(bool connected);

}  // namespace ash::glanceables_util

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_UTIL_H_
