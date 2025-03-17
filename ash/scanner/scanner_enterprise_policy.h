// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_ENTERPRISE_POLICY_H_
#define ASH_SCANNER_SCANNER_ENTERPRISE_POLICY_H_

#include "ash/ash_export.h"

namespace ash {

// Valid integer values for `ash::prefs::kScannerEnterprisePolicyAllowed`.
//
// Any integer value outside of this range should be treated equivalently to
// `kAllowedWithoutModelImprovement` (the default for managed users), as invalid
// values can only be achieved if an administrator mistakenly sets it.
enum class ASH_EXPORT ScannerEnterprisePolicy : int {
  kAllowedWithModelImprovement = 0,
  kAllowedWithoutModelImprovement = 1,
  kDisallowed = 2,
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_ENTERPRISE_POLICY_H_
