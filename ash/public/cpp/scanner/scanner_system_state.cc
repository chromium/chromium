// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/scanner/scanner_system_state.h"

namespace ash {

ScannerSystemState::ScannerSystemState(ScannerStatus status,
                                       SystemChecks failed_checks)
    : status(status), failed_checks(failed_checks) {}

ScannerSystemState::~ScannerSystemState() = default;

}  // namespace ash
