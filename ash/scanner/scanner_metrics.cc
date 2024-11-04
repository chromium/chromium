// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace ash {

constexpr char kScannerFeatureUserStateHistogramName[] =
    "Ash.ScannerFeature.UserState";

void RecordScannerFeatureUserState(ScannerFeatureUserState state) {
  base::UmaHistogramEnumeration(kScannerFeatureUserStateHistogramName, state);
}

}  // namespace ash
