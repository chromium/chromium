// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_store.h"

#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"

namespace safe_browsing {
using leveldb_proto::ProtoDatabaseProvider;

const char kNamespace[] = "SafeBrowsing";
const char kDbName[] = "NotificationTelemetry";

NotificationTelemetryStore::NotificationTelemetryStore(
    std::unique_ptr<ProtoDatabase<CSBRR::ServiceWorkerBehavior>>
        service_worker_behavior_db)
    : service_worker_behavior_db_(std::move(service_worker_behavior_db)) {
  service_worker_behavior_db_->Init(base::BindOnce(
      &NotificationTelemetryStore::OnInitDone, weak_ptr_factory_.GetWeakPtr()));
}
NotificationTelemetryStore::NotificationTelemetryStore() = default;
NotificationTelemetryStore::NotificationTelemetryStore(Profile* profile) {
  ProtoDatabaseProvider* db_provider =
      profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider();

  base::FilePath safe_browsing_dir = profile->GetPath().AppendASCII(kNamespace);
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  service_worker_behavior_db_ =
      db_provider->GetDB<CSBRR::ServiceWorkerBehavior>(
          leveldb_proto::ProtoDbType::NOTIFICATION_TELEMETRY_STORE,
          safe_browsing_dir.AppendASCII(kDbName), db_task_runner);

  service_worker_behavior_db_->Init(base::BindOnce(
      &NotificationTelemetryStore::OnInitDone, weak_ptr_factory_.GetWeakPtr()));
}

NotificationTelemetryStore::~NotificationTelemetryStore() = default;

void NotificationTelemetryStore::AddServiceWorkerRegistrationBehavior(
    const GURL& scope_url,
    const std::vector<GURL>& import_script_urls,
    SuccessCallback success_callback) {
  std::unique_ptr<CSBRR::ServiceWorkerBehavior> service_worker_behavior =
      std::make_unique<CSBRR::ServiceWorkerBehavior>();
  service_worker_behavior->set_scope_url(scope_url.spec());
  for (const GURL& import_script_url : import_script_urls) {
    service_worker_behavior->add_import_script_urls(import_script_url.spec());
  }
  AddServiceWorkerBehavior(std::move(service_worker_behavior),
                           std::move(success_callback));
}

void NotificationTelemetryStore::AddServiceWorkerPushBehavior(
    const GURL& script_url,
    const std::vector<GURL>& requested_urls,
    SuccessCallback success_callback) {
  std::unique_ptr<CSBRR::ServiceWorkerBehavior> service_worker_behavior =
      std::make_unique<CSBRR::ServiceWorkerBehavior>();
  service_worker_behavior->set_script_url(script_url.spec());
  for (const GURL& requested_url : requested_urls) {
    service_worker_behavior->add_requested_urls(requested_url.spec());
  }
  AddServiceWorkerBehavior(std::move(service_worker_behavior),
                           std::move(success_callback));
}

void NotificationTelemetryStore::GetServiceWorkerBehaviors(
    LoadEntriesCallback load_entries_callback,
    SuccessCallback success_callback) {
  // If not initialized queue the call and return early.
  if (!initialized_) {
    queued_db_callbacks_.emplace(base::BindOnce(
        &NotificationTelemetryStore::GetServiceWorkerBehaviors,
        weak_ptr_factory_.GetWeakPtr(), std::move(load_entries_callback),
        std::move(success_callback)));
    return;
  }
  service_worker_behavior_db_->LoadEntries(base::BindOnce(
      &NotificationTelemetryStore::OnLoadEntries,
      weak_ptr_factory_.GetWeakPtr(), std::move(load_entries_callback),
      std::move(success_callback)));
}

void NotificationTelemetryStore::DeleteAll(SuccessCallback success_callback) {
  // If not initialized queue the call and return early.
  if (!initialized_) {
    queued_db_callbacks_.emplace(base::BindOnce(
        &NotificationTelemetryStore::DeleteAll, weak_ptr_factory_.GetWeakPtr(),
        std::move(success_callback)));
    return;
  }
  service_worker_behavior_db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<
          ProtoDatabase<CSBRR::ServiceWorkerBehavior>::KeyEntryVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      base::BindOnce(&NotificationTelemetryStore::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback)));
}

void NotificationTelemetryStore::OnInitDone(
    leveldb_proto::Enums::InitStatus status) {
  initialized_ = status == leveldb_proto::Enums::InitStatus::kOK;
  if (!initialized_) {
    return;
  }
  while (!queued_db_callbacks_.empty()) {
    std::move(queued_db_callbacks_.front()).Run();
    queued_db_callbacks_.pop();
  }
}

void NotificationTelemetryStore::OnUpdateEntries(
    SuccessCallback success_callback,
    bool success) {
  std::move(success_callback).Run(success);
}

void NotificationTelemetryStore::OnLoadEntries(
    LoadEntriesCallback callback,
    SuccessCallback success_callback,
    bool success,
    std::unique_ptr<std::vector<CSBRR::ServiceWorkerBehavior>> entries) {
  std::move(callback).Run(success, std::move(entries));
  std::move(success_callback).Run(success);
}

void NotificationTelemetryStore::AddServiceWorkerBehavior(
    std::unique_ptr<CSBRR::ServiceWorkerBehavior> service_worker_behavior,
    SuccessCallback success_callback) {
  if (!initialized_) {
    queued_db_callbacks_.emplace(base::BindOnce(
        &NotificationTelemetryStore::AddServiceWorkerBehavior,
        weak_ptr_factory_.GetWeakPtr(), std::move(service_worker_behavior),
        std::move(success_callback)));
    return;
  }
  auto entries = std::make_unique<
      ProtoDatabase<CSBRR::ServiceWorkerBehavior>::KeyEntryVector>();
  entries->emplace_back(
      base::NumberToString(base::Time::Now().InMillisecondsFSinceUnixEpoch()),
      *std::move(service_worker_behavior));
  service_worker_behavior_db_->UpdateEntries(
      std::move(entries), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&NotificationTelemetryStore::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback)));
}

bool NotificationTelemetryStore::IsInitializedForTest() {
  return initialized_;
}

}  // namespace safe_browsing
