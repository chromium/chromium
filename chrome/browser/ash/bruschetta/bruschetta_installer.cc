// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"

namespace bruschetta {

const char16_t* BruschettaInstallErrorString(
    const BruschettaInstallError error) {
#define USTR(s) u##s
#define ENTRY(name)                  \
  case BruschettaInstallError::name: \
    return USTR(#name)
  switch (error) {
    ENTRY(kUnknown);
    ENTRY(kInstallationProhibited);
    ENTRY(kDlcInstallError);
    ENTRY(kDownloadError);
    ENTRY(kInvalidFirmware);
    ENTRY(kInvalidBootDisk);
    ENTRY(kInvalidPflash);
    ENTRY(kUnableToOpenImages);
    ENTRY(kCreateDiskError);
    ENTRY(kStartVmFailed);
  }
#undef ENTRY
#undef USTR
  return u"unknown code";
}

}  // namespace bruschetta
