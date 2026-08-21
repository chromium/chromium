// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_URL_ALLOWLIST_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_URL_ALLOWLIST_H_

#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "url/gurl.h"

namespace arc {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ArcContentUrlAccessCheckResult)
enum class ArcContentUrlAccessCheckResult {
  kDenied = 0,
  kCacheHit = 1,
  kDatabaseHit = 2,
  kMaxValue = kDatabaseHit,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/arc/enums.xml)

// Encapsulates the two-level content URL allowlist:
// 1. A synchronous in-memory LRU cache on the UI thread for the fast-path.
// 2. A background SQLite database on a sequenced thread pool for
//    fallback/persistence.
class ArcContentUrlAllowlist {
 public:
  static constexpr size_t kLruCacheDefaultMaxSize = 1024;

  // `db_path` specifies the file path on disk where the SQLite database is
  // persisted. If `db_path` is empty, the database is opened in-memory
  // (typically used for testing).
  explicit ArcContentUrlAllowlist(
      const base::FilePath& db_path,
      size_t max_cache_size = kLruCacheDefaultMaxSize);

  ArcContentUrlAllowlist(const ArcContentUrlAllowlist&) = delete;
  ArcContentUrlAllowlist& operator=(const ArcContentUrlAllowlist&) = delete;

  ~ArcContentUrlAllowlist();

  // Grants access to the specified content URL.
  void GrantAccess(const GURL& url);

  // Checks if the specified URL is present in the allowlist.
  // Performs a fast-path cache check synchronously. If missed, performs a
  // background database query asynchronously and invokes `callback` with the
  // result.
  void IsAccessGranted(const GURL& url,
                       base::OnceCallback<void(bool)> callback);

 private:
  // A persistent SQLite database used to store the allowlist of Android
  // content:// URLs that have been explicitly granted access by the browser.
  // This class must be accessed on a sequenced background task runner.
  class Database {
   public:
    explicit Database(const base::FilePath& db_path);

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    ~Database();

    // Initializes the database.
    bool Initialize();

    // Adds a URL to the database.
    void AddUrl(const GURL& url);

    // Checks if a URL is in the database.
    bool IsUrlPresent(const GURL& url);

   private:
    SEQUENCE_CHECKER(sequence_checker_);

    const base::FilePath db_path_;
    sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
    sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);
  };

  void OnDatabaseAccessCheckComplete(const GURL& url,
                                     base::OnceCallback<void(bool)> callback,
                                     bool is_present);

  SEQUENCE_CHECKER(sequence_checker_);

  base::LRUCacheSet<GURL> lru_cache_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::SequenceBound<Database> database_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<ArcContentUrlAllowlist> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_URL_ALLOWLIST_H_
