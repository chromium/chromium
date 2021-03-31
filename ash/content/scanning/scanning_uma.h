// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_SCANNING_SCANNING_UMA_H_
#define ASH_CONTENT_SCANNING_SCANNING_UMA_H_

namespace chromeos {
namespace scanning {

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml.

enum class ScanCompleteAction {
  kDoneButtonClicked = 0,
  kFilesAppOpened = 1,
  kMediaAppOpened = 2,
  kMaxValue = kMediaAppOpened,
};

enum class ScanJobFailureReason {
  kUnknownScannerError = 0,
  kScannerNotFound = 1,
  kUnsupportedScanToPath = 2,
  kSaveToDiskFailed = 3,
  kMaxValue = kSaveToDiskFailed,
};

enum class ScanJobSettingsResolution {
  kUnexpectedDpi = 0,
  k75Dpi = 1,
  k100Dpi = 2,
  k150Dpi = 3,
  k200Dpi = 4,
  k300Dpi = 5,
  k600Dpi = 6,
  kMaxValue = k600Dpi,
};

// Converts resolution integer value to a ScanJobSettingsResolution enum value.
ScanJobSettingsResolution GetResolutionEnumValue(const int resolution);

}  // namespace scanning
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when //ash/content/scanning
// moved to ash
namespace ash {
namespace scanning {
using ::chromeos::scanning::ScanJobFailureReason;
}  // namespace scanning
}  // namespace ash

#endif  // ASH_CONTENT_SCANNING_SCANNING_UMA_H_
