// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/task/thread_pool.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/access_context_audit_database.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

AccessContextAuditService::CookieAccessHelper::CookieAccessHelper(
    AccessContextAuditService* service)
    : service_(service) {
  DCHECK(service);
  deletion_observation_.Observe(service);
}

AccessContextAuditService::CookieAccessHelper::~CookieAccessHelper() {
  FlushCookieRecords();
}

void AccessContextAuditService::CookieAccessHelper::OnCookieDeleted(
    const net::CanonicalCookie& cookie) {
  accessed_cookies_.erase(cookie);
}

void AccessContextAuditService::CookieAccessHelper::RecordCookieAccess(
    const net::CookieList& accessed_cookies,
    const url::Origin& top_frame_origin) {
  if (top_frame_origin != last_seen_top_frame_origin_) {
    FlushCookieRecords();
    last_seen_top_frame_origin_ = top_frame_origin;
  }

  for (const auto& cookie : accessed_cookies)
    accessed_cookies_.insert(cookie);
}

void AccessContextAuditService::CookieAccessHelper::FlushCookieRecords() {
  if (accessed_cookies_.empty())
    return;

  service_->RecordCookieAccess(accessed_cookies_, last_seen_top_frame_origin_);
  accessed_cookies_.clear();
}

AccessContextAuditService::AccessContextAuditService(Profile* profile)
    : clock_(base::DefaultClock::GetInstance()), profile_(profile) {}

AccessContextAuditService::~AccessContextAuditService() {
  // This destructor may do I/O, so destroy it on the database task runner.
  database_task_runner_->ReleaseSoon(FROM_HERE, std::move(database_));
}

bool AccessContextAuditService::Init(
    const base::FilePath& database_dir,
    network::mojom::CookieManager* cookie_manager,
    history::HistoryService* history_service,
    content::StoragePartition* storage_partition) {
  database_ = base::MakeRefCounted<AccessContextAuditDatabase>(database_dir);

  // Tests may have provided a task runner already.
  if (!database_task_runner_) {
    // Task runner is set to block shutdown as content settings are checked on
    // service shutdown and records which should not be persisted are removed.
    database_task_runner_ =
        base::ThreadPool::CreateUpdateableSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::ThreadPolicy::PREFER_BACKGROUND,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }

  if (!database_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&AccessContextAuditDatabase::Init, database_,
                         profile_->ShouldRestoreOldSessionCookies()))) {
    return false;
  }

  cookie_manager->AddGlobalChangeListener(
      cookie_listener_receiver_.BindNewPipeAndPassRemote());
  history_observation_.Observe(history_service);
  storage_partition_observation_.Observe(storage_partition);
  return true;
}

void AccessContextAuditService::RecordCookieAccess(
    const canonical_cookie::CookieHashSet& accessed_cookies,
    const url::Origin& top_frame_origin) {
  // Opaque top frame origins are not supported.
  if (top_frame_origin.opaque())
    return;

  auto now = clock_->Now();
  std::vector<AccessContextAuditDatabase::AccessRecord> access_records;
  for (const auto& cookie : accessed_cookies) {
    // Do not record accesses to already expired cookies. This service is
    // informed of deletion via OnCookieChange.
    if (cookie.ExpiryDate() < now && cookie.IsPersistent())
      continue;

    access_records.emplace_back(top_frame_origin, cookie.Name(),
                                cookie.Domain(), cookie.Path(), now,
                                cookie.IsPersistent());
  }
  database_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AccessContextAuditDatabase::AddRecords,
                                database_, std::move(access_records)));
}

void AccessContextAuditService::RecordStorageAPIAccess(
    const url::Origin& storage_origin,
    AccessContextAuditDatabase::StorageAPIType type,
    const url::Origin& top_frame_origin) {
  // Opaque top frame origins are only supported for storing cross-site storage
  // access records after history deletions.
  if (top_frame_origin.opaque())
    return;
  DCHECK(!storage_origin.opaque());

  std::vector<AccessContextAuditDatabase::AccessRecord> access_record = {
      AccessContextAuditDatabase::AccessRecord(top_frame_origin, type,
                                               storage_origin, clock_->Now())};
  database_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AccessContextAuditDatabase::AddRecords,
                                database_, std::move(access_record)));
}

void AccessContextAuditService::GetCookieAccessRecords(
    AccessContextRecordsCallback callback) {
  if (!user_visible_tasks_in_progress++)
    database_task_runner_->UpdatePriority(base::TaskPriority::USER_VISIBLE);

  for (auto& helper : cookie_access_helpers_)
    helper.FlushCookieRecords();

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AccessContextAuditDatabase::GetCookieRecords, database_),
      base::BindOnce(
          &AccessContextAuditService::CompleteGetAccessRecordsInternal,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AccessContextAuditService::GetStorageAccessRecords(
    AccessContextRecordsCallback callback) {
  if (!user_visible_tasks_in_progress++)
    database_task_runner_->UpdatePriority(base::TaskPriority::USER_VISIBLE);

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AccessContextAuditDatabase::GetStorageRecords, database_),
      base::BindOnce(
          &AccessContextAuditService::CompleteGetAccessRecordsInternal,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

namespace {

bool IsSameSite(const url::Origin& origin1, const url::Origin& origin2) {
  return net::registry_controlled_domains::SameDomainOrHost(
      origin1, origin2,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

void SelectThirdPartyStorageAccessRecords(
    AccessContextRecordsCallback callback,
    std::vector<AccessContextAuditDatabase::AccessRecord> storage_records) {
  std::vector<AccessContextAuditDatabase::AccessRecord> result;
  for (auto& record : storage_records) {
    if (!IsSameSite(record.origin, record.top_frame_origin))
      result.push_back(std::move(record));
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace

void AccessContextAuditService::GetThirdPartyStorageAccessRecords(
    AccessContextRecordsCallback callback) {
  GetStorageAccessRecords(base::BindOnce(&SelectThirdPartyStorageAccessRecords,
                                         std::move(callback)));
}

void AccessContextAuditService::GetAllAccessRecords(
    AccessContextRecordsCallback callback) {
  if (!user_visible_tasks_in_progress++)
    database_task_runner_->UpdatePriority(base::TaskPriority::USER_VISIBLE);

  for (auto& helper : cookie_access_helpers_)
    helper.FlushCookieRecords();

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AccessContextAuditDatabase::GetAllRecords, database_),
      base::BindOnce(
          &AccessContextAuditService::CompleteGetAccessRecordsInternal,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AccessContextAuditService::CompleteGetAccessRecordsInternal(
    AccessContextRecordsCallback callback,
    std::vector<AccessContextAuditDatabase::AccessRecord> records) {
  DCHECK_GT(user_visible_tasks_in_progress, 0);
  if (!--user_visible_tasks_in_progress)
    database_task_runner_->UpdatePriority(base::TaskPriority::BEST_EFFORT);

  std::move(callback).Run(std::move(records));
}

void AccessContextAuditService::RemoveAllRecordsForOriginKeyedStorage(
    const url::Origin& origin,
    AccessContextAuditDatabase::StorageAPIType type) {
  DCHECK_NE(type, AccessContextAuditDatabase::StorageAPIType::kCookie)
      << "Cookies are not an origin keyed storage type.";
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AccessContextAuditDatabase::RemoveAllRecordsForOriginKeyedStorage,
          database_, std::move(origin), type));
}

void AccessContextAuditService::Shutdown() {
  DCHECK(cookie_access_helpers_.empty());
  ClearSessionOnlyRecords();
}

void AccessContextAuditService::OnStorageKeyDataCleared(
    uint32_t remove_mask,
    content::StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    const base::Time begin,
    const base::Time end) {
  std::set<AccessContextAuditDatabase::StorageAPIType> types;

  if (remove_mask & content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS)
    types.insert(AccessContextAuditDatabase::StorageAPIType::kFileSystem);
  if (remove_mask & content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB)
    types.insert(AccessContextAuditDatabase::StorageAPIType::kIndexedDB);
  if (remove_mask & content::StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE)
    types.insert(AccessContextAuditDatabase::StorageAPIType::kLocalStorage);
  if (remove_mask & content::StoragePartition::REMOVE_DATA_MASK_WEBSQL)
    types.insert(AccessContextAuditDatabase::StorageAPIType::kWebDatabase);
  if (remove_mask & content::StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS)
    types.insert(AccessContextAuditDatabase::StorageAPIType::kServiceWorker);
  if (remove_mask & content::StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE)
    types.insert(AccessContextAuditDatabase::StorageAPIType::kCacheStorage);

  if (types.empty())
    return;

  DCHECK_EQ(AccessContextAuditDatabase::StorageAPIType::kMaxValue,
            AccessContextAuditDatabase::StorageAPIType::kAppCacheDeprecated)
      << "Unexpected number of storage types. Ensure that all storage types "
         "are accounted for when checking |remove_mask|.";
  bool all_origin_storage_types = types.size() == 7;

  if (begin == base::Time() && end == base::Time::Max() &&
      !storage_key_matcher && all_origin_storage_types) {
    database_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AccessContextAuditDatabase::RemoveAllRecords,
                                  database_));
    return;
  }

  if (!storage_key_matcher && all_origin_storage_types) {
    database_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AccessContextAuditDatabase::RemoveAllRecordsForTimeRange,
            database_, begin, end));
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AccessContextAuditDatabase::RemoveStorageApiRecords,
                     database_, types, std::move(storage_key_matcher), begin,
                     end));
}

void AccessContextAuditService::OnCookieChange(
    const net::CookieChangeInfo& change) {
  switch (change.cause) {
    case net::CookieChangeCause::INSERTED:
    case net::CookieChangeCause::OVERWRITE:
      // Ignore change causes that do not represent deletion.
      return;
    case net::CookieChangeCause::EXPLICIT:
    case net::CookieChangeCause::UNKNOWN_DELETION:
    case net::CookieChangeCause::EXPIRED:
    case net::CookieChangeCause::EVICTED:
    case net::CookieChangeCause::EXPIRED_OVERWRITE: {
      // Notify helpers so that future accesses to this cookie are reported.
      for (auto& helper : cookie_access_helpers_) {
        helper.OnCookieDeleted(change.cookie);
      }
      // Remove records of deleted cookie from database.
      database_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&AccessContextAuditDatabase::RemoveAllRecordsForCookie,
                         database_, change.cookie.Name(),
                         change.cookie.Domain(), change.cookie.Path()));
    }
  }
}

void AccessContextAuditService::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    database_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AccessContextAuditDatabase::RemoveAllRecordsHistory,
                       database_));
    return;
  }

  if (deletion_info.time_range().IsValid()) {
    // If a time range is specified, a time based deletion is performed as a
    // first pass before origins without history entries are removed. A second
    // pass based on origins is required as access record timestamps are not
    // directly comparable to history timestamps. Only deleting based on
    // timestamp may persist origins on disk for which no other trace exists.
    database_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AccessContextAuditDatabase::RemoveAllRecordsForTimeRangeHistory,
            database_, deletion_info.time_range().begin(),
            deletion_info.time_range().end()));
  }

  std::vector<url::Origin> deleted_origins;
  // Map is of type {Origin -> {Count, LastVisitTime}}.
  for (const auto& origin_urls_remaining :
       deletion_info.deleted_urls_origin_map()) {
    if (origin_urls_remaining.second.first > 0)
      continue;
    deleted_origins.emplace_back(
        url::Origin::Create(origin_urls_remaining.first));
  }

  if (deleted_origins.size() > 0) {
    database_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AccessContextAuditDatabase::RemoveAllRecordsForTopFrameOrigins,
            database_, std::move(deleted_origins)));
  }
}

void AccessContextAuditService::AddObserver(CookieAccessHelper* helper) {
  cookie_access_helpers_.AddObserver(helper);
}

void AccessContextAuditService::RemoveObserver(CookieAccessHelper* helper) {
  cookie_access_helpers_.RemoveObserver(helper);
}

void AccessContextAuditService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void AccessContextAuditService::SetTaskRunnerForTesting(
    scoped_refptr<base::UpdateableSequencedTaskRunner> task_runner) {
  DCHECK(!database_task_runner_);
  database_task_runner_ = std::move(task_runner);
}

void AccessContextAuditService::ClearSessionOnlyRecords() {
  ContentSettingsForOneType settings;
  HostContentSettingsMapFactory::GetForProfile(profile_)->GetSettingsForOneType(
      ContentSettingsType::COOKIES, &settings);

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AccessContextAuditDatabase::RemoveSessionOnlyRecords,
                     database_, std::move(settings)));
}
