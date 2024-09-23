// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCANNER_SCANNER_ENUMS_H_
#define ASH_PUBLIC_CPP_SCANNER_SCANNER_ENUMS_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Specifies any errors returned from the scanner server.
enum class ASH_PUBLIC_EXPORT ScannerError {
  kUnknownError,
};

// Specifies the enabled / disabled state of the feature.
enum class ASH_PUBLIC_EXPORT ScannerStatus {
  kEnabled,
  kBlocked,
};

// Specifies a constraint required for the feature to function.
enum class ASH_PUBLIC_EXPORT ScannerSystemCheck {
  kMinValue = 0,
  kConsentRequired = kMinValue,
  kMaxValue = kConsentRequired,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_ENUMS_H_
