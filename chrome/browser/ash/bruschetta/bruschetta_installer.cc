// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"

namespace bruschetta {

const char16_t* BruschettaInstallResultString(
    const BruschettaInstallResult error) {
#define USTR(s) u##s
#define ENTRY(name)                   \
  case BruschettaInstallResult::name: \
    return USTR(#name)
  switch (error) {
    ENTRY(kUnknown);
    ENTRY(kSuccess);
    ENTRY(kInstallationProhibited);
    ENTRY(kDownloadError);
    ENTRY(kInvalidBootDisk);
    ENTRY(kInvalidPflash);
    ENTRY(kUnableToOpenImages);
    ENTRY(kCreateDiskError);
    ENTRY(kStartVmFailed);
    ENTRY(kInstallPflashError);
    ENTRY(kVmAlreadyExists);
    ENTRY(kClearVekFailed);
    ENTRY(kToolsDlcOfflineError);
    ENTRY(kToolsDlcNeedUpdateError);
    ENTRY(kToolsDlcNeedRebootError);
    ENTRY(kToolsDlcDiskFullError);
    ENTRY(kToolsDlcBusyError);
    ENTRY(kToolsDlcUnknownError);
    ENTRY(kFirmwareDlcOfflineError);
    ENTRY(kFirmwareDlcNeedUpdateError);
    ENTRY(kFirmwareDlcNeedRebootError);
    ENTRY(kFirmwareDlcDiskFullError);
    ENTRY(kFirmwareDlcBusyError);
    ENTRY(kFirmwareDlcUnknownError);
    ENTRY(kConciergeUnavailableError);
    ENTRY(kNotEnoughMemoryError);
    ENTRY(kNoAdidError);
  }
#undef ENTRY
#undef USTR
  return u"unknown code";
}

}  // namespace bruschetta
