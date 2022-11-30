// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_DATABASE_H_
#define CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_DATABASE_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "url/origin.h"

// Provides the backend SQLite storage to support access context auditing. This
// requires storing information associating individual client-side storage API
// accesses (e.g. cookies, indexedDBs, etc.) with the top level frame origins
// at the time of their access.
class AccessContextAuditDatabase
    : public base::RefCountedThreadSafe<AccessContextAuditDatabase> {
 public:
  // All client-side storage API types supported by the database.
  enum class StorageAPIType : int {
    kCookie = 0,
    kLocalStorage = 1,
    kSessionStorage = 2,
    kFileSystem = 3,
    kWebDatabase = 4,
    kServiceWorker = 5,
    kCacheStorage = 6,
    kIndexedDB = 7,
    kAppCacheDeprecated = 8,
    kMaxValue = kAppCacheDeprecated
  };

  // An individual record of a Storage API access, associating the individual
  // API usage with a top level frame origin.
  struct AccessRecord {
    AccessRecord(const url::Origin& top_frame_origin,
                 const std::string& name,
                 const std::string& domain,
                 const std::string& path,
                 const base::Time& last_access_time,
                 bool is_persistent);
    AccessRecord(const url::Origin& top_frame_origin,
                 const StorageAPIType& type,
                 const url::Origin& origin,
                 const base::Time& last_access_time);
    ~AccessRecord();
    AccessRecord(const AccessRecord& other);
    AccessRecord& operator=(const AccessRecord& other);

    url::Origin top_frame_origin;
    StorageAPIType type;

    // Identifies a canonical cookie, only used when |type| is kCookie.
    std::string name;
    std::string domain;
    std::string path;

    // Identifies an origin-keyed storage API, used when |type| is NOT kCookie.
    url::Origin origin;

    base::Time last_access_time;

    // When |type| is kCookie, indicates the record will be cleared on startup
    // unless the database is started with restore_non_persistent_cookies.
    bool is_persistent;
  };

  explicit AccessContextAuditDatabase(
      const base::FilePath& path_to_database_dir);

  // Initialises internal database. Must be called prior to any other usage.
  void Init(bool restore_non_persistent_cookies);

  // Calculates and reports various database metrics.
  void ComputeDatabaseMetrics();

  // Persists the provided list of |records| in the database.
  void AddRecords(const std::vector<AccessRecord>& records);

  // Returns all cookie entries in the database. No ordering is enforced.
  std::vector<AccessRecord> GetCookieRecords();

  // Returns all storage entries in the database. No ordering is enforced.
  std::vector<AccessRecord> GetStorageRecords();

  // Returns all entries in the database. No ordering is enforced.
  std::vector<AccessRecord> GetAllRecords();

  // Removes a record from the database and from future calls to GetAllRecords.
  void RemoveRecord(const AccessRecord& record);

  // Removes all records from the the database.
  void RemoveAllRecords();

  // Remove all records from the database from a history deletion.
  // Unlike RemoveAllRecords, this method keeps a record of cross-site storage
  // access but replaces the top-level origin with an opaque origin. This is due
  // to the fact that we use cross-site storage access records to clear
  // third-party storage when a user manually clears third-party cookies.
  void RemoveAllRecordsHistory();

  // Removes all records where |begin| <= record.last_access_time <= |end|.
  void RemoveAllRecordsForTimeRange(base::Time begin, base::Time end);

  // Removes all records where |begin| <= record.last_access_time <= |end| from
  // a history deletion. Like RemoveAllRecordsHistory, we keep cross-site
  // storage access records and make the top-level origin opaque when user
  // controls for third-party data clearing is enabled.
  void RemoveAllRecordsForTimeRangeHistory(base::Time begin, base::Time end);

  // Removes all records that match the provided cookie details.
  void RemoveAllRecordsForCookie(const std::string& name,
                                 const std::string& domain,
                                 const std::string& path);

  // Remove all records of access to |origin|'s storage API of |type|.
  void RemoveAllRecordsForOriginKeyedStorage(const url::Origin& origin,
                                             StorageAPIType type);

  // Remove all records with a top frame origin present in |origins|.
  void RemoveAllRecordsForTopFrameOrigins(
      const std::vector<url::Origin>& origins);

  // Removes all records for which the result of inspecting |content_settings|
  // for the storage origin or cookie domain is a content setting of
  // CLEAR_ON_EXIT.
  void RemoveSessionOnlyRecords(
      const ContentSettingsForOneType& content_settings);

  // Remove storage API access records for which the storage type is a member of
  // `storage_api_types`, the timestamp is between `begin` and `end`, and the
  // `storage_key_matcher` callback, if set, returns true for the storage key.
  void RemoveStorageApiRecords(
      const std::set<StorageAPIType>& storage_api_types,
      content::StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
      base::Time begin,
      base::Time end);

 protected:
  virtual ~AccessContextAuditDatabase() = default;

 private:
  friend class base::RefCountedThreadSafe<AccessContextAuditDatabase>;
  bool InitializeSchema();

  std::vector<AccessRecord> GetStorageRecordsForTopFrameOrigins(
      const std::vector<url::Origin>& origins);
  std::vector<AccessRecord> GetStorageRecordsForTimeRange(base::Time begin,
                                                          base::Time end);

  sql::Database db_;
  sql::MetaTable meta_table_;
  base::FilePath db_file_path_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_DATABASE_H_
