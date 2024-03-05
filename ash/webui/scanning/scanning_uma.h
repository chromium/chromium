// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNING_SCANNING_UMA_H_
#define ASH_WEBUI_SCANNING_SCANNING_UMA_H_

namespace ash::scanning {

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/metadata/scanning/enums.xml.
enum class ScanCompleteAction {
  kDoneButtonClicked = 0,
  kFilesAppOpened = 1,
  kMediaAppOpened = 2,
  kMaxValue = kMediaAppOpened,
};

enum class ScanMultiPageToolbarAction {
  kRemovePage = 0,
  kRescanPage = 1,
  kMaxValue = kRescanPage,
};

enum class ScanJobFailureReason {
  kUnknownScannerError = 0,
  kScannerNotFound = 1,
  kUnsupportedScanToPath = 2,
  kSaveToDiskFailed = 3,
  kDeviceBusy = 4,
  kAdfJammed = 5,
  kAdfEmpty = 6,
  kFlatbedOpen = 7,
  kIoError = 8,
  kSuccess = 9,
  kMaxValue = kSuccess,
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

}  // namespace ash::scanning

#endif  // ASH_WEBUI_SCANNING_SCANNING_UMA_H_
