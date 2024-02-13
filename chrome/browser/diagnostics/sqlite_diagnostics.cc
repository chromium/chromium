// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/sqlite_diagnostics.h"

#include <stdint.h>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "components/history/core/browser/history_constants.h"
#include "components/webdata/common/webdata_constants.h"
#include "content/public/common/content_constants.h"
#include "sql/database.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "storage/browser/database/database_tracker.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace diagnostics {

namespace {

// Generic diagnostic test class for checking SQLite database integrity.
class SqliteIntegrityTest : public DiagnosticsTest {
 public:
  // These are bit flags, so each value should be a power of two.
  enum Flags {
    NO_FLAGS_SET = 0,
    CRITICAL = 0x01,
    REMOVE_IF_CORRUPT = 0x02,
  };

  SqliteIntegrityTest(uint32_t flags,
                      DiagnosticsTestId id,
                      const base::FilePath& db_path)
      : DiagnosticsTest(id), flags_(flags), db_path_(db_path) {}

  SqliteIntegrityTest(const SqliteIntegrityTest&) = delete;
  SqliteIntegrityTest& operator=(const SqliteIntegrityTest&) = delete;

  bool RecoveryImpl(DiagnosticsModel::Observer* observer) override {
    int outcome_code = GetOutcomeCode();
    if (flags_ & REMOVE_IF_CORRUPT) {
      switch (outcome_code) {
        case DIAG_SQLITE_ERROR_HANDLER_CALLED:
        case DIAG_SQLITE_CANNOT_OPEN_DB:
        case DIAG_SQLITE_DB_LOCKED:
        case DIAG_SQLITE_PRAGMA_FAILED:
        case DIAG_SQLITE_DB_CORRUPTED:
          LOG(WARNING) << "Removing broken SQLite database: "
                       << db_path_.value();
          sql::Database::Delete(db_path_);
          break;
        case DIAG_SQLITE_SUCCESS:
        case DIAG_SQLITE_FILE_NOT_FOUND_OK:
        case DIAG_SQLITE_FILE_NOT_FOUND:
          break;
        default:
          DCHECK(false) << "Invalid outcome code: " << outcome_code;
          break;
      }
    }
    return true;
  }

  bool ExecuteImpl(DiagnosticsModel::Observer* observer) override {
    // If we're given an absolute path, use it. If not, then assume it's under
    // the profile directory.
    base::FilePath path;
    if (!db_path_.IsAbsolute())
      path = GetUserDefaultProfileDir().Append(db_path_);
    else
      path = db_path_;

    if (!base::PathExists(path)) {
      if (flags_ & CRITICAL) {
        RecordOutcome(DIAG_SQLITE_FILE_NOT_FOUND,
                      "File not found",
                      DiagnosticsModel::TEST_FAIL_CONTINUE);
      } else {
        RecordOutcome(DIAG_SQLITE_FILE_NOT_FOUND_OK,
                      "File not found (but that is OK)",
                      DiagnosticsModel::TEST_OK);
      }
      return true;
    }

    int errors = 0;
    {  // Scope the statement and database so they close properly.
      sql::Database database({.page_size = 4096, .cache_size = 500});
      scoped_refptr<ErrorRecorder> recorder(new ErrorRecorder);

      // Set the error callback so that we can get useful results in a debug
      // build for a corrupted database. Without setting the error callback,
      // sql::Database will just DCHECK.
      database.set_error_callback(base::BindRepeating(
          &SqliteIntegrityTest::ErrorRecorder::RecordSqliteError,
          recorder->AsWeakPtr(), &database));
      if (!database.Open(path)) {
        RecordFailure(DIAG_SQLITE_CANNOT_OPEN_DB,
                      "Cannot open DB. Possibly corrupted");
        return true;
      }
      if (recorder->has_error()) {
        RecordFailure(DIAG_SQLITE_ERROR_HANDLER_CALLED,
                      recorder->FormatError());
        return true;
      }
      sql::Statement statement(
          database.GetUniqueStatement("PRAGMA integrity_check;"));
      if (recorder->has_error()) {
        RecordFailure(DIAG_SQLITE_ERROR_HANDLER_CALLED,
                      recorder->FormatError());
        return true;
      }
      if (!statement.is_valid()) {
        int error = database.GetErrorCode();
        if (static_cast<int>(sql::SqliteResultCode::kBusy) == error) {
          RecordFailure(DIAG_SQLITE_DB_LOCKED,
                        "Database locked by another process");
        } else {
          std::string str("Pragma failed. Error: ");
          str += base::NumberToString(error);
          RecordFailure(DIAG_SQLITE_PRAGMA_FAILED, str);
        }
        return false;
      }

      while (statement.Step()) {
        std::string result(statement.ColumnString(0));
        if ("ok" != result)
          ++errors;
      }
      if (recorder->has_error()) {
        RecordFailure(DIAG_SQLITE_ERROR_HANDLER_CALLED,
                      recorder->FormatError());
        return true;
      }
    }

    // All done. Report to the user.
    if (errors != 0) {
      std::string str("Database corruption detected: ");
      str += base::NumberToString(errors) + " errors";
      RecordFailure(DIAG_SQLITE_DB_CORRUPTED, str);
      return true;
    }
    RecordSuccess("No corruption detected");
    return true;
  }

 private:
  class ErrorRecorder : public base::RefCounted<ErrorRecorder> {
   public:
    ErrorRecorder() = default;

    ErrorRecorder(const ErrorRecorder&) = delete;
    ErrorRecorder& operator=(const ErrorRecorder&) = delete;

    void RecordSqliteError(sql::Database* connection,
                           int sqlite_error,
                           sql::Statement* statement) {
      has_error_ = true;
      sqlite_error_ = sqlite_error;
      last_errno_ = connection->GetLastErrno();
      message_ = connection->GetErrorMessage();
    }

    bool has_error() const { return has_error_; }

    std::string FormatError() {
      return base::StringPrintf("SQLite error: %d, Last Errno: %d: %s",
                                sqlite_error_,
                                last_errno_,
                                message_.c_str());
    }

    base::WeakPtr<ErrorRecorder> AsWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    friend class base::RefCounted<ErrorRecorder>;
    ~ErrorRecorder() {}

    bool has_error_ = false;
    int sqlite_error_ = 0;
    int last_errno_ = 0;
    std::string message_;
    base::WeakPtrFactory<ErrorRecorder> weak_ptr_factory_{this};
  };

  uint32_t flags_;
  base::FilePath db_path_;
};

}  // namespace

std::unique_ptr<DiagnosticsTest> MakeSqliteCookiesDbTest() {
  return std::make_unique<SqliteIntegrityTest>(
      SqliteIntegrityTest::CRITICAL, DIAGNOSTICS_SQLITE_INTEGRITY_COOKIE_TEST,
      base::FilePath(chrome::kCookieFilename));
}

std::unique_ptr<DiagnosticsTest> MakeSqliteWebDatabaseTrackerDbTest() {
  base::FilePath databases_dir(storage::kDatabaseDirectoryName);
  base::FilePath tracker_db =
      databases_dir.Append(storage::kTrackerDatabaseFileName);
  return std::make_unique<SqliteIntegrityTest>(
      SqliteIntegrityTest::NO_FLAGS_SET,
      DIAGNOSTICS_SQLITE_INTEGRITY_DATABASE_TRACKER_TEST, tracker_db);
}

std::unique_ptr<DiagnosticsTest> MakeSqliteHistoryDbTest() {
  return std::make_unique<SqliteIntegrityTest>(
      SqliteIntegrityTest::CRITICAL, DIAGNOSTICS_SQLITE_INTEGRITY_HISTORY_TEST,
      base::FilePath(history::kHistoryFilename));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<DiagnosticsTest> MakeSqliteNssCertDbTest() {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);
  return std::make_unique<SqliteIntegrityTest>(
      SqliteIntegrityTest::REMOVE_IF_CORRUPT,
      DIAGNOSTICS_SQLITE_INTEGRITY_NSS_CERT_TEST,
      home_dir.Append(ash::kNssCertDbPath));
}

std::unique_ptr<DiagnosticsTest> MakeSqliteNssKeyDbTest() {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);
  return std::make_unique<SqliteIntegrityTest>(
      SqliteIntegrityTest::REMOVE_IF_CORRUPT,
      DIAGNOSTICS_SQLITE_INTEGRITY_NSS_KEY_TEST,
      home_dir.Append(ash::kNssKeyDbPath));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<DiagnosticsTest> MakeSqliteFaviconsDbTest() {
  return std::make_unique<SqliteIntegrityTest>(
      SqliteIntegrityTest::NO_FLAGS_SET,
      DIAGNOSTICS_SQLITE_INTEGRITY_FAVICONS_TEST,
      base::FilePath(history::kFaviconsFilename));
}

std::unique_ptr<DiagnosticsTest> MakeSqliteTopSitesDbTest() {
  return std::make_unique<SqliteIntegrityTest>(
      SqliteIntegrityTest::NO_FLAGS_SET,
      DIAGNOSTICS_SQLITE_INTEGRITY_TOPSITES_TEST,
      base::FilePath(history::kTopSitesFilename));
}

std::unique_ptr<DiagnosticsTest> MakeSqliteWebDataDbTest() {
  return std::make_unique<SqliteIntegrityTest>(
      SqliteIntegrityTest::CRITICAL, DIAGNOSTICS_SQLITE_INTEGRITY_WEB_DATA_TEST,
      base::FilePath(kWebDataFilename));
}

}  // namespace diagnostics
