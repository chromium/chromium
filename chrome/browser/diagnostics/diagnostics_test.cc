// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/diagnostics/diagnostics_test.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"

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

DiagnosticsTest::DiagnosticsTest(DiagnosticsTestId id)
    : id_(id), outcome_code_(-1), result_(DiagnosticsModel::TEST_NOT_RUN) {}

DiagnosticsTest::~DiagnosticsTest() {}

bool DiagnosticsTest::Execute(DiagnosticsModel::Observer* observer,
                              DiagnosticsModel* model,
                              size_t index) {
  start_time_ = base::Time::Now();
  result_ = DiagnosticsModel::TEST_RUNNING;
  bool keep_going = ExecuteImpl(observer);
  if (observer)
    observer->OnTestFinished(index, model);
  return keep_going;
}

bool DiagnosticsTest::Recover(DiagnosticsModel::Observer* observer,
                              DiagnosticsModel* model,
                              size_t index) {
  result_ = DiagnosticsModel::RECOVERY_RUNNING;
  bool keep_going = RecoveryImpl(observer);
  result_ = keep_going ? DiagnosticsModel::RECOVERY_OK
                       : DiagnosticsModel::RECOVERY_FAIL_STOP;
  if (observer)
    observer->OnRecoveryFinished(index, model);
  return keep_going;
}

void DiagnosticsTest::RecordOutcome(int outcome_code,
                                    const std::string& additional_info,
                                    DiagnosticsModel::TestResult result) {
  end_time_ = base::Time::Now();
  outcome_code_ = outcome_code;
  additional_info_ = additional_info;
  result_ = result;
}

// static
base::FilePath DiagnosticsTest::GetUserDefaultProfileDir() {
  base::FilePath path;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &path))
    return base::FilePath();
  return path.AppendASCII(chrome::kInitialProfile);
}

int DiagnosticsTest::GetId() const { return id_; }

std::string DiagnosticsTest::GetName() const {
  return std::string(FindTestInfo(id_)->name);
}

std::string DiagnosticsTest::GetTitle() const {
  return std::string(FindTestInfo(id_)->description);
}

DiagnosticsModel::TestResult DiagnosticsTest::GetResult() const {
  return result_;
}

int DiagnosticsTest::GetOutcomeCode() const { return outcome_code_; }

std::string DiagnosticsTest::GetAdditionalInfo() const {
  return additional_info_;
}

base::Time DiagnosticsTest::GetStartTime() const { return start_time_; }

base::Time DiagnosticsTest::GetEndTime() const { return end_time_; }

bool DiagnosticsTest::RecoveryImpl(DiagnosticsModel::Observer* observer) {
  return true;
}

}  // namespace diagnostics
