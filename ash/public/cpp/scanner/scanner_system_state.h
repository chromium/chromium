// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCANNER_SCANNER_SYSTEM_STATE_H_
#define ASH_PUBLIC_CPP_SCANNER_SCANNER_SYSTEM_STATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "base/containers/enum_set.h"

namespace ash {

// Holds the current system state, including any failed checks. This can be
// used to derive the feature's enablement state.
struct ASH_PUBLIC_EXPORT ScannerSystemState {
  using SystemChecks = base::EnumSet<ScannerSystemCheck,
                                     ScannerSystemCheck::kMinValue,
                                     ScannerSystemCheck::kMaxValue>;

  ScannerSystemState(ScannerStatus status, SystemChecks failed_checks);
  ~ScannerSystemState();

  ScannerStatus status;
  SystemChecks failed_checks;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_SYSTEM_STATE_H_
