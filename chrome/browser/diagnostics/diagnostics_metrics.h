// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_METRICS_H_
#define CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_METRICS_H_

#include <string>

namespace diagnostics {

// Test IDs used to indicate in UMA stats which diagnostics fail, and also to
// look up string identifiers for tests. If you add an ID here, you will also
// need to add corresponding strings to several things in the .cc file.
enum DiagnosticsTestId {
  DIAGNOSTICS_CONFLICTING_DLLS_TEST,
  DIAGNOSTICS_DISK_SPACE_TEST,
  DIAGNOSTICS_INSTALL_TYPE_TEST,
  DIAGNOSTICS_JSON_BOOKMARKS_TEST,
  DIAGNOSTICS_JSON_LOCAL_STATE_TEST,
  DIAGNOSTICS_JSON_PREFERENCES_TEST,
  DIAGNOSTICS_OPERATING_SYSTEM_TEST,
  DIAGNOSTICS_PATH_DICTIONARIES_TEST,
  DIAGNOSTICS_PATH_LOCAL_STATE_TEST,
  DIAGNOSTICS_PATH_RESOURCES_TEST,
  DIAGNOSTICS_PATH_USER_DATA_TEST,
  DIAGNOSTICS_VERSION_TEST,
  DIAGNOSTICS_SQLITE_INTEGRITY_APP_CACHE_TEST,
  DIAGNOSTICS_SQLITE_INTEGRITY_ARCHIVED_HISTORY_TEST_OBSOLETE,
  DIAGNOSTICS_SQLITE_INTEGRITY_COOKIE_TEST,
  DIAGNOSTICS_SQLITE_INTEGRITY_DATABASE_TRACKER_TEST,
  DIAGNOSTICS_SQLITE_INTEGRITY_HISTORY_TEST,
  DIAGNOSTICS_SQLITE_INTEGRITY_NSS_CERT_TEST,
  DIAGNOSTICS_SQLITE_INTEGRITY_NSS_KEY_TEST,
  DIAGNOSTICS_SQLITE_INTEGRITY_THUMBNAILS_TEST_OBSOLETE,
  DIAGNOSTICS_SQLITE_INTEGRITY_WEB_DATA_TEST,
  DIAGNOSTICS_SQLITE_INTEGRITY_FAVICONS_TEST,
  DIAGNOSTICS_SQLITE_INTEGRITY_TOPSITES_TEST,
  // Add new entries immediately above this comment. Do not reorder or renumber
  // the entries, as they are tied to historical enum values in the UMA stats.
  // If you add an entry, you will need to also add an entry to kTestNameInfo,
  // and to the TEST_CASES macro in the .cc.

  // This must always be last in the list.
  DIAGNOSTICS_TEST_ID_COUNT
};

// Possible enum values for individual test metrics.
enum RunResultMetrics {
  RESULT_NOT_RUN,
  RESULT_SUCCESS,
  RESULT_FAILURE,
  RESULT_SKIPPED,
  // Add new items above this line.
  RESULT_COUNT
};

// Returns the string identifier of a test |id|. It will only contain
// characters [A-Za-z0-9] with no spaces.
std::string GetTestName(DiagnosticsTestId id);

// Returns the string description of a test |id|. This is not a localized
// string. It is only meant for developer consumption, because this function
// will be called before the localization services are initialized.
std::string GetTestDescription(DiagnosticsTestId id);

// These record an UMA metric for the given test or recovery operation.
void RecordUMARecoveryResult(DiagnosticsTestId id, RunResultMetrics result);
void RecordUMATestResult(DiagnosticsTestId id, RunResultMetrics result);

}  // namespace diagnostics

#endif  // CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_METRICS_H_
