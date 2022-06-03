// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/switches.h"

namespace switches {

// Simulates a specific HRESULT error code returned by the update check.
// If the switch value is not specified (as hex) then it defaults to E_FAIL.
const char kSimulateUpdateHresult[] = "simulate-update-hresult";

// Simulates a GoogleUpdateErrorCode error by the update check.
// Must be supplied with |kSimulateUpdateHresult| switch.
const char kSimulateUpdateErrorCode[] = "simulate-update-error-code";

}  // namespace switches
