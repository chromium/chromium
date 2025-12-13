// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_STORE_H_
#define CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_STORE_H_

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_store_interface.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

class Profile;
using leveldb_proto::ProtoDatabase;

namespace safe_browsing {

class NotificationTelemetryStore : public NotificationTelemetryStoreInterface {
 public:
  explicit NotificationTelemetryStore(Profile* profile);
  ~NotificationTelemetryStore() override;
  NotificationTelemetryStore(const NotificationTelemetryStore&) = delete;
  NotificationTelemetryStore& operator=(const NotificationTelemetryStore&) =
      delete;

  void AddServiceWorkerRegistrationBehavior(
      const GURL& scope_url,
      const std::vector<GURL>& import_script_urls,
      SuccessCallback success_callback) override;

  void AddServiceWorkerPushBehavior(const GURL& script_url,
                                    const std::vector<GURL>& requested_urls,
                                    SuccessCallback success_callback) override;

  void GetServiceWorkerBehaviors(LoadEntriesCallback load_entries_callback,
                                 SuccessCallback success_callback) override;
  void DeleteAll(SuccessCallback success_callback) override;

  bool IsInitializedForTest();

 protected:
  // Used only for testing.
  explicit NotificationTelemetryStore(
      std::unique_ptr<ProtoDatabase<CSBRR::ServiceWorkerBehavior>>
          service_worker_behavior_db);
  // Used only for testing.
  NotificationTelemetryStore();

 private:
  // Callback for ProtoDatabase::Init.
  void OnInitDone(leveldb_proto::Enums::InitStatus status);

  // Callback for ProtoDatabase::UpdateEntries.
  void OnUpdateEntries(SuccessCallback success_callback, bool success);

  // Callback for ProtoDatabase::LoadEntries.
  void OnLoadEntries(
      LoadEntriesCallback load_entries_callback,
      SuccessCallback success_callback,
      bool success,
      std::unique_ptr<std::vector<CSBRR::ServiceWorkerBehavior>> entries);

  // Add ServiceWorkerBehavior into the ProtoDatabase.
  void AddServiceWorkerBehavior(
      std::unique_ptr<CSBRR::ServiceWorkerBehavior> service_worker_behavior,
      SuccessCallback success_callback);

  // Manages the persistence of ServiceWorkerBehavior messages.
  std::unique_ptr<ProtoDatabase<CSBRR::ServiceWorkerBehavior>>
      service_worker_behavior_db_;
  // Database calls to be executed once the database is initialized.
  base::queue<base::OnceClosure> queued_db_callbacks_;
  // Indicates whether the database is initialized.
  bool initialized_ = false;

  base::WeakPtrFactory<NotificationTelemetryStore> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_STORE_H_
