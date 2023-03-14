// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_RECON_DIAGNOSTICS_H_
#define CHROME_BROWSER_DIAGNOSTICS_RECON_DIAGNOSTICS_H_

#include <memory>

#include "chrome/browser/diagnostics/diagnostics_test.h"

namespace diagnostics {

enum OutcomeCodes {
  DIAG_RECON_SUCCESS,

  // OperatingSystemTest
  DIAG_RECON_PRE_WINDOW_XP_SP2,

  // InstallTypeTest
  DIAG_RECON_INSTALL_PATH_PROVIDER_OBSOLETE,  // Retained to preserve numbering.

  // VersionTest
  DIAG_RECON_NO_VERSION,
  DIAG_RECON_EMPTY_VERSION,

  // PathTest
  DIAG_RECON_DEPENDENCY,
  DIAG_RECON_PATH_PROVIDER,
  DIAG_RECON_PATH_NOT_FOUND,
  DIAG_RECON_CANNOT_OBTAIN_SIZE,
  DIAG_RECON_FILE_TOO_LARGE,
  DIAG_RECON_NOT_WRITABLE,

  // DiskSpaceTest
  DIAG_RECON_UNABLE_TO_QUERY,
  DIAG_RECON_LOW_DISK_SPACE,

  // JSONTest
  DIAG_RECON_FILE_NOT_FOUND,
  DIAG_RECON_FILE_NOT_FOUND_OK,
  DIAG_RECON_CANNOT_OBTAIN_FILE_SIZE,
  DIAG_RECON_FILE_TOO_BIG,
  DIAG_RECON_UNABLE_TO_OPEN_FILE,
  DIAG_RECON_PARSE_ERROR,
};

std::unique_ptr<DiagnosticsTest> MakeLocalOrSyncableBookmarksTest();
std::unique_ptr<DiagnosticsTest> MakeAccountBookmarksTest();
std::unique_ptr<DiagnosticsTest> MakeDictonaryDirTest();
std::unique_ptr<DiagnosticsTest> MakeDiskSpaceTest();
std::unique_ptr<DiagnosticsTest> MakeInstallTypeTest();
std::unique_ptr<DiagnosticsTest> MakeLocalStateFileTest();
std::unique_ptr<DiagnosticsTest> MakeLocalStateTest();
std::unique_ptr<DiagnosticsTest> MakeOperatingSystemTest();
std::unique_ptr<DiagnosticsTest> MakePreferencesTest();
std::unique_ptr<DiagnosticsTest> MakeResourcesFileTest();
std::unique_ptr<DiagnosticsTest> MakeUserDirTest();
std::unique_ptr<DiagnosticsTest> MakeVersionTest();

}  // namespace diagnostics

#endif  // CHROME_BROWSER_DIAGNOSTICS_RECON_DIAGNOSTICS_H_
