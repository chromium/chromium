// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_metrics.h"

#include <ostream>
#include <string>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "chrome/browser/diagnostics/diagnostics_test.h"

namespace diagnostics {

namespace {

// A struct to hold information about the tests.
struct TestNameInfo {
  // Should only contain characters [A-Za-z0-9] (no spaces).
  const char* name;

  // A non-localized description only meant for developer consumption.
  const char* description;
};

// This structure MUST have DIAGNOSTICS_TEST_COUNT entries in it: one for each
// value of DiagnosticsTestId.  Note that the values in the "name" fields are
// used for UMA metrics names (with "Diagnostics.Test." or
// "Diagnostics.Recovery." prepended), so do not change them without
// understanding the consequences.
const TestNameInfo kTestNameInfo[] = {
    {"ConflictingDlls", "Conflicting modules"},
    {"DiskSpace", "Available disk space"},
    {"InstallType", "Install type"},
    {"JSONBookmarks", "Bookmark file"},
    {"JSONLocalState", "Local state integrity"},
    {"JSONPreferences", "User preferences integrity"},
    {"OperatingSystem", "Operating system supported version"},
    {"PathDictionaries", "App dictionaries directory path"},
    {"PathLocalState", "Local state path"},
    {"PathResources", "Resources path"},
    {"PathUserData", "User data path"},
    {"Version", "Chrome version test"},
    {"SQLiteIntegrityAppCache", "Application cache database"},
    {"SQLiteIntegrityArchivedHistory", "Archived history database (obsolete)"},
    {"SQLiteIntegrityCookie", "Cookie database"},
    {"SQLiteIntegrityDatabaseTracker", "Database tracker database"},
    {"SQLiteIntegrityHistory", "History database"},
    {"SQLiteIntegrityNSSCert", "NSS certificate database"},
    {"SQLiteIntegrityNSSKey", "NSS Key database"},
    {"SQLiteIntegrityThumbnails", "Thumbnails database (obsolete)"},
    {"SQLiteIntegrityWebData", "Web Data database"},
    {"SQLiteIntegrityFavicons", "Favicons database"},
    {"SQLiteIntegrityTopSites", "Top Sites database"},
    // Add new entries in the same order as DiagnosticsTestId.
};

static_assert(std::size(kTestNameInfo) == DIAGNOSTICS_TEST_ID_COUNT,
              "diagnostics test info mismatch");

const TestNameInfo* FindTestInfo(DiagnosticsTestId id) {
  DCHECK(id < DIAGNOSTICS_TEST_ID_COUNT);
  return &kTestNameInfo[id];
}

}  // namespace

std::string GetTestName(DiagnosticsTestId id) {
  return std::string(FindTestInfo(id)->name);
}

std::string GetTestDescription(DiagnosticsTestId id) {
  return std::string(FindTestInfo(id)->description);
}

#define TEST_CASE(name, id)                                \
  case id:                                                 \
    UMA_HISTOGRAM_ENUMERATION(name, result, RESULT_COUNT); \
    break

// These must each have their own complete case so that the UMA macros create
// a unique static pointer block for each individual metric.  This is done as a
// macro to prevent errors where the ID is added to one function below, but not
// the other, because they must match.
#define TEST_CASES(name)                                               \
  TEST_CASE(name, DIAGNOSTICS_CONFLICTING_DLLS_TEST);                  \
  TEST_CASE(name, DIAGNOSTICS_DISK_SPACE_TEST);                        \
  TEST_CASE(name, DIAGNOSTICS_INSTALL_TYPE_TEST);                      \
  TEST_CASE(name, DIAGNOSTICS_JSON_BOOKMARKS_TEST);                    \
  TEST_CASE(name, DIAGNOSTICS_JSON_LOCAL_STATE_TEST);                  \
  TEST_CASE(name, DIAGNOSTICS_JSON_PREFERENCES_TEST);                  \
  TEST_CASE(name, DIAGNOSTICS_OPERATING_SYSTEM_TEST);                  \
  TEST_CASE(name, DIAGNOSTICS_PATH_DICTIONARIES_TEST);                 \
  TEST_CASE(name, DIAGNOSTICS_PATH_LOCAL_STATE_TEST);                  \
  TEST_CASE(name, DIAGNOSTICS_PATH_RESOURCES_TEST);                    \
  TEST_CASE(name, DIAGNOSTICS_PATH_USER_DATA_TEST);                    \
  TEST_CASE(name, DIAGNOSTICS_VERSION_TEST);                           \
  TEST_CASE(name, DIAGNOSTICS_SQLITE_INTEGRITY_APP_CACHE_TEST);        \
  TEST_CASE(name, DIAGNOSTICS_SQLITE_INTEGRITY_ARCHIVED_HISTORY_TEST_OBSOLETE);\
  TEST_CASE(name, DIAGNOSTICS_SQLITE_INTEGRITY_COOKIE_TEST);           \
  TEST_CASE(name, DIAGNOSTICS_SQLITE_INTEGRITY_DATABASE_TRACKER_TEST); \
  TEST_CASE(name, DIAGNOSTICS_SQLITE_INTEGRITY_HISTORY_TEST);          \
  TEST_CASE(name, DIAGNOSTICS_SQLITE_INTEGRITY_NSS_CERT_TEST);         \
  TEST_CASE(name, DIAGNOSTICS_SQLITE_INTEGRITY_NSS_KEY_TEST);          \
  TEST_CASE(name, DIAGNOSTICS_SQLITE_INTEGRITY_THUMBNAILS_TEST_OBSOLETE);\
  TEST_CASE(name, DIAGNOSTICS_SQLITE_INTEGRITY_WEB_DATA_TEST);         \
  TEST_CASE(name, DIAGNOSTICS_SQLITE_INTEGRITY_FAVICONS_TEST);         \
  TEST_CASE(name, DIAGNOSTICS_SQLITE_INTEGRITY_TOPSITES_TEST);

void RecordUMARecoveryResult(DiagnosticsTestId id, RunResultMetrics result) {
  const std::string name("Diagnostics.Recovery." +
                         GetTestName(static_cast<DiagnosticsTestId>(id)));
  switch (id) {
    TEST_CASES(name);  // See above
    default:
      NOTREACHED() << "Unhandled UMA Metric type" << id;
  }
}

void RecordUMATestResult(DiagnosticsTestId id, RunResultMetrics result) {
  const std::string name("Diagnostics.Test." +
                         GetTestName(static_cast<DiagnosticsTestId>(id)));
  switch (id) {
    TEST_CASES(name);  // See above
    default:
      NOTREACHED() << "Unhandled UMA Metric type" << id;
  }
}
#undef TEST_CASE
#undef TEST_CASES

}  // namespace diagnostics
