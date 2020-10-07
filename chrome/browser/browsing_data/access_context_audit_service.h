// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_
#define CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_

#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/browsing_data/access_context_audit_database.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/content/canonical_cookie_hash.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

typedef base::OnceCallback<void(
    std::vector<AccessContextAuditDatabase::AccessRecord>)>
    AccessContextRecordsCallback;

class AccessContextAuditService
    : public KeyedService,
      public ::network::mojom::CookieChangeListener,
      public history::HistoryServiceObserver,
      public content::StoragePartition::DataRemovalObserver {
 public:
  class CookieAccessHelper;
  void AddObserver(CookieAccessHelper* helper);
  void RemoveObserver(CookieAccessHelper* helper);

  // A helper class used to report cookie accesses to the audit service. Keeps
  // an internal record of cookie accesses which have already been seen.
  // Repeated calls to RecordCookieAccess are ignored until the cookie is
  // observed as deleted, or the set of seen cookies is cleared via
  // ClearSeenCookies.
  class CookieAccessHelper : public base::CheckedObserver {
   public:
    explicit CookieAccessHelper(AccessContextAuditService* service);
    ~CookieAccessHelper() override;

    // Selectively forwards cookie accesses to the audit service based on
    // whether this helper has previously seen the cookie.
    void RecordCookieAccess(const net::CookieList& accessed_cookies,
                            const url::Origin& top_frame_origin);

    // Observer method called by the audit service when a cookie has been
    // deleted and future accesses should be reported.
    void OnCookieDeleted(const net::CanonicalCookie& cookie);

    // Resets the internal set of seen cookies, resulting in future reported
    // accesses to those cookies being forwarded to the service for recording.
    // This should be called at least prior to every top-frame navigation,
    // calling more frequently increases accuracy of access timestamps but also
    // increases performance overhead.
    void ClearSeenCookies();

   private:
    AccessContextAuditService* service_;
    canonical_cookie::CookieHashSet seen_cookies_;
    ScopedObserver<AccessContextAuditService, CookieAccessHelper>
        deletion_observer_{this};
  };

  explicit AccessContextAuditService(Profile* profile);
  ~AccessContextAuditService() override;

  // Initialises the Access Context Audit database in |database_dir|, and
  // attaches listeners to |cookie_manager| and |history_service|.
  bool Init(const base::FilePath& database_dir,
            network::mojom::CookieManager* cookie_manager,
            history::HistoryService* history_service,
            content::StoragePartition* storage_partition);

  // Records access for |storage_origin|'s storage of |type| against
  // |top_frame_origin|.
  void RecordStorageAPIAccess(const url::Origin& storage_origin,
                              AccessContextAuditDatabase::StorageAPIType type,
                              const url::Origin& top_frame_origin);

  // Queries database for all access context records, which are provided via
  // |callback|.
  void GetAllAccessRecords(AccessContextRecordsCallback callback);

  // Called on completion of GetAllAccessRecords.
  void CompleteGetAllAccessRecordsInternal(
      AccessContextRecordsCallback callback,
      std::vector<AccessContextAuditDatabase::AccessRecord> records);

  // Remove all records of access to |origin|'s storage API of |type|.
  void RemoveAllRecordsForOriginKeyedStorage(
      const url::Origin& origin,
      AccessContextAuditDatabase::StorageAPIType type);

  // KeyedService:
  void Shutdown() override;

  // StoragePartition::DataRemovalObserver:
  void OnOriginDataCleared(
      uint32_t remove_mask,
      base::RepeatingCallback<bool(const url::Origin&)> origin_matcher,
      const base::Time begin,
      const base::Time end) override;

  // ::network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo& change) override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // Override the internal clock used to record storage API access timestamps
  // and check for expired cookies.
  void SetClockForTesting(base::Clock* clock);

  // Override internal task runner with provided task runner. Must be called
  // before Init().
  void SetTaskRunnerForTesting(
      scoped_refptr<base::UpdateableSequencedTaskRunner> task_runner);

 private:
  friend class AccessContextAuditServiceTest;
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, CookieRecords);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, ExpiredCookies);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, HistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, AllHistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest,
                           TimeRangeHistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, OpaqueOrigins);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, SessionOnlyRecords);

  // Records accesses for all cookies in |details| against |top_frame_origin|.
  // Should only be accessed via the CookieAccessHelper.
  void RecordCookieAccess(const net::CookieList& accessed_cookies,
                          const url::Origin& top_frame_origin);

  // Removes any records which are session only from the database.
  void ClearSessionOnlyRecords();

  scoped_refptr<AccessContextAuditDatabase> database_;
  scoped_refptr<base::UpdateableSequencedTaskRunner> database_task_runner_;

  int user_visible_tasks_in_progress = 0;

  base::Clock* clock_;
  Profile* profile_;

  base::ObserverList<CookieAccessHelper> cookie_access_helpers_;

  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};
  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_observer_{this};
  ScopedObserver<content::StoragePartition,
                 content::StoragePartition::DataRemovalObserver>
      storage_partition_observer_{this};

  base::WeakPtrFactory<AccessContextAuditService> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AccessContextAuditService);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_
