// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_
#define CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "chrome/browser/browsing_data/access_context_audit_database.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/content/canonical_cookie_hash.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

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
  // an in-memory set of cookie accesses which are flushed to the audit service
  // when a different top_frame_origin is provided, or when the helper is
  // destroyed. Helpers should not outlive the audit service, this is DCHECK
  // enforced on audit service shutdown.
  class CookieAccessHelper : public base::CheckedObserver {
   public:
    explicit CookieAccessHelper(AccessContextAuditService* service);
    ~CookieAccessHelper() override;

    // Adds the list of |accessed_cookies| to the in memory set of accessed
    // cookies. If |top_frame_origin| has a different value than previously
    // provided to this function, then first the set of accessed cookies is
    // flushed to the database and cleared.
    void RecordCookieAccess(const net::CookieList& accessed_cookies,
                            const url::Origin& top_frame_origin);

    // Observer method called by the audit service when a cookie has been
    // deleted and should be removed from the in-memory set of accessed cookies.
    void OnCookieDeleted(const net::CanonicalCookie& cookie);

   private:
    friend class AccessContextAuditService;
    FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, CookieAccessHelper);

    // Clear the in-memory set of accessed cookies after passing them to the
    // audit service for persisting to disk.
    void FlushCookieRecords();

    raw_ptr<AccessContextAuditService> service_;
    canonical_cookie::CookieHashSet accessed_cookies_;
    url::Origin last_seen_top_frame_origin_;
    base::ScopedObservation<AccessContextAuditService, CookieAccessHelper>
        deletion_observation_{this};
  };

  explicit AccessContextAuditService(Profile* profile);

  AccessContextAuditService(const AccessContextAuditService&) = delete;
  AccessContextAuditService& operator=(const AccessContextAuditService&) =
      delete;

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

  // Queries database for all access context records for cookies, which are
  // provided via |callback|.
  void GetCookieAccessRecords(AccessContextRecordsCallback callback);

  // Queries database for all access context records for storage, which are
  // provided via |callback|.
  void GetStorageAccessRecords(AccessContextRecordsCallback callback);

  // Queries database for all access context records for storage that are
  // accessed in a 3P context, which are provided via |callback|.
  void GetThirdPartyStorageAccessRecords(AccessContextRecordsCallback callback);

  // Queries database for all access context records, which are provided via
  // |callback|.
  void GetAllAccessRecords(AccessContextRecordsCallback callback);

  // Remove all records of access to |origin|'s storage API of |type|.
  void RemoveAllRecordsForOriginKeyedStorage(
      const url::Origin& origin,
      AccessContextAuditDatabase::StorageAPIType type);

  // KeyedService:
  void Shutdown() override;

  // StoragePartition::DataRemovalObserver:
  void OnStorageKeyDataCleared(
      uint32_t remove_mask,
      content::StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
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
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, GetStorageRecords);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest,
                           GetThirdPartyStorageRecords);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, HistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, AllHistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest,
                           TimeRangeHistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, OpaqueOrigins);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, SessionOnlyRecords);

  friend class AccessContextAuditThirdPartyDataClearingTest;
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditThirdPartyDataClearingTest,
                           HistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditThirdPartyDataClearingTest,
                           AllHistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditThirdPartyDataClearingTest,
                           TimeRangeHistoryDeletion);

  // Records accesses for all cookies in |details| against |top_frame_origin|.
  // Should only be accessed via the CookieAccessHelper.
  void RecordCookieAccess(
      const canonical_cookie::CookieHashSet& accessed_cookies,
      const url::Origin& top_frame_origin);

  // Removes any records which are session only from the database.
  void ClearSessionOnlyRecords();

  // Called on completion of GetCookieRecords, GetStorageRecords, or
  // GetAllAccessRecords.
  void CompleteGetAccessRecordsInternal(
      AccessContextRecordsCallback callback,
      std::vector<AccessContextAuditDatabase::AccessRecord> records);

  scoped_refptr<AccessContextAuditDatabase> database_;
  scoped_refptr<base::UpdateableSequencedTaskRunner> database_task_runner_;

  int user_visible_tasks_in_progress = 0;

  raw_ptr<base::Clock> clock_;
  raw_ptr<Profile> profile_;

  base::ObserverList<CookieAccessHelper> cookie_access_helpers_;

  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};
  base::ScopedObservation<content::StoragePartition,
                          content::StoragePartition::DataRemovalObserver>
      storage_partition_observation_{this};

  base::WeakPtrFactory<AccessContextAuditService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_
