// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_content_url_allowlist.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace arc {

namespace {

constexpr int kCurrentVersionNumber = 1;
constexpr int kCompatibleVersionNumber = 1;

constexpr char kUrlsCreateTableSql[] =
    "CREATE TABLE IF NOT EXISTS urls ("
    "url TEXT NOT NULL PRIMARY KEY"
    ")";

constexpr char kInsertUrlSql[] = "INSERT OR IGNORE INTO urls (url) VALUES (?)";

constexpr char kSelectUrlSql[] = "SELECT 1 FROM urls WHERE url = ?";

}  // namespace

ArcContentUrlAllowlist::Database::Database(const base::FilePath& db_path)
    : db_path_(db_path), db_(sql::Database::Tag("ArcContentUrlDatabase")) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ArcContentUrlAllowlist::Database::~Database() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool ArcContentUrlAllowlist::Database::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!db_.is_open());

  if (db_path_.empty()) {
    if (!db_.OpenInMemory()) {
      LOG(ERROR) << "Failed to open in-memory database.";
      return false;
    }
  } else {
    const base::FilePath dir = db_path_.DirName();
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(dir, &error)) {
      LOG(ERROR) << "Failed to create directory for database: " << dir.value()
                 << " Error: " << base::File::ErrorToString(error);
      return false;
    }

    // Delete any existing database file on session startup to prevent the
    // database from growing indefinitely across sessions.
    base::DeleteFile(db_path_);

    if (!db_.Open(db_path_)) {
      LOG(ERROR) << "Failed to open database: " << db_path_.value();
      return false;
    }
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    LOG(ERROR) << "Failed to begin transaction.";
    return false;
  }

  if (!db_.Execute(kUrlsCreateTableSql)) {
    LOG(ERROR) << "Failed to create urls table.";
    return false;
  }

  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    LOG(ERROR) << "Failed to initialize meta table.";
    return false;
  }

  return transaction.Commit();
}

void ArcContentUrlAllowlist::Database::AddUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_.is_open()) {
    return;
  }

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertUrlSql));
  statement.BindString(0, url.spec());
  if (!statement.Run()) {
    LOG(ERROR) << "Failed to add URL to ArcContentUrlDatabase: " << url.spec();
  }
}

bool ArcContentUrlAllowlist::Database::IsUrlPresent(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_.is_open()) {
    return false;
  }

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectUrlSql));
  statement.BindString(0, url.spec());

  if (!statement.Step()) {
    if (!statement.Succeeded()) {
      LOG(ERROR) << "Failed to query URL from ArcContentUrlDatabase: "
                 << url.spec();
    }
    return false;
  }
  return true;
}

ArcContentUrlAllowlist::ArcContentUrlAllowlist(const base::FilePath& db_path,
                                               size_t max_cache_size)
    : lru_cache_(max_cache_size),
      database_(base::ThreadPool::CreateSequencedTaskRunner(
                    {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                     base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                db_path) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  database_.AsyncCall(&Database::Initialize)
      .Then(base::BindOnce([](bool success) {
        if (!success) {
          LOG(ERROR) << "Failed to initialize ArcContentUrlDatabase";
        }
      }));
}

ArcContentUrlAllowlist::~ArcContentUrlAllowlist() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ArcContentUrlAllowlist::GrantAccess(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!url.is_valid()) {
    LOG(WARNING) << "GrantAccess called with invalid URL.";
    return;
  }

  // If already in cache, Put/Get will refresh its recency.
  if (lru_cache_.Get(url) != lru_cache_.end()) {
    return;
  }

  lru_cache_.Put(GURL(url));

  // Persist to the database asynchronously.
  database_.AsyncCall(&Database::AddUrl).WithArgs(url);
}

void ArcContentUrlAllowlist::IsAccessGranted(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // 1. Cache Hit.
  if (lru_cache_.Get(url) != lru_cache_.end()) {
    base::UmaHistogramEnumeration("Arc.FileSystem.ContentUrlAccessCheckResult",
                                  ArcContentUrlAccessCheckResult::kCacheHit);
    std::move(callback).Run(true);
    return;
  }

  // 2. Cache Miss - Query Database.
  database_.AsyncCall(&Database::IsUrlPresent)
      .WithArgs(url)
      .Then(base::BindOnce(
          &ArcContentUrlAllowlist::OnDatabaseAccessCheckComplete,
          weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void ArcContentUrlAllowlist::OnDatabaseAccessCheckComplete(
    const GURL& url,
    base::OnceCallback<void(bool)> callback,
    bool is_present) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_present) {
    VLOG(1) << "URL found in ArcContentUrlDatabase.";
    base::UmaHistogramEnumeration("Arc.FileSystem.ContentUrlAccessCheckResult",
                                  ArcContentUrlAccessCheckResult::kDatabaseHit);
    // Put in cache.
    lru_cache_.Put(GURL(url));
  } else {
    LOG(WARNING) << "Content URL access denied (not in ArcContentUrlAllowlist)";
    base::UmaHistogramEnumeration("Arc.FileSystem.ContentUrlAccessCheckResult",
                                  ArcContentUrlAccessCheckResult::kDenied);
  }
  std::move(callback).Run(is_present);
}

}  // namespace arc
