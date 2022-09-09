// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_
#define CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/diagnostics/diagnostics_test.h"

namespace diagnostics {

enum SQLiteIntegrityOutcomeCode {
  DIAG_SQLITE_SUCCESS,
  DIAG_SQLITE_FILE_NOT_FOUND_OK,
  DIAG_SQLITE_FILE_NOT_FOUND,
  DIAG_SQLITE_ERROR_HANDLER_CALLED,
  DIAG_SQLITE_CANNOT_OPEN_DB,
  DIAG_SQLITE_DB_LOCKED,
  DIAG_SQLITE_PRAGMA_FAILED,
  DIAG_SQLITE_DB_CORRUPTED
};

// Factories for the database integrity tests we run in diagnostic mode.
std::unique_ptr<DiagnosticsTest> MakeSqliteCookiesDbTest();
std::unique_ptr<DiagnosticsTest> MakeSqliteFaviconsDbTest();
std::unique_ptr<DiagnosticsTest> MakeSqliteHistoryDbTest();
std::unique_ptr<DiagnosticsTest> MakeSqliteTopSitesDbTest();

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<DiagnosticsTest> MakeSqliteNssCertDbTest();
std::unique_ptr<DiagnosticsTest> MakeSqliteNssKeyDbTest();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<DiagnosticsTest> MakeSqliteWebDatabaseTrackerDbTest();
std::unique_ptr<DiagnosticsTest> MakeSqliteWebDataDbTest();

}  // namespace diagnostics

#endif  // CHROME_BROWSER_DIAGNOSTICS_SQLITE_DIAGNOSTICS_H_
