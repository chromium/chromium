// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEDERATED_LEARNING_FLOC_EVENT_LOGGER_H_
#define CHROME_BROWSER_FEDERATED_LEARNING_FLOC_EVENT_LOGGER_H_

#include <stddef.h>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/driver/sync_service_observer.h"

namespace syncer {
class UserEventService;
class SyncService;
}  // namespace syncer

namespace federated_learning {

class FlocRemotePermissionService;

// Provide an interface to log the FlocIdComputed event after each time floc is
// computed. For each logging request, a floc is eligible to be logged if the
// following conditons are met:
// 1) Sync & sync-history are enabled.
// 2) Supplemental Web and App Activity is enabled.
// 3) Supplemental Ad Personalization is enabled.
// 4) The account type is NOT a child account.
//
// Given that the sync service is often ready a few moments after the browser
// start, but the floc may already have been computed before then, for practical
// purposes, each request that fails the initial sync-history check will be
// given a second chance 10 seconds later.
class FlocEventLogger {
 public:
  struct Event {
    bool sim_hash_computed = false;
    uint64_t sim_hash = 0;
    base::Time time;
  };

  using CanLogEventCallback = base::OnceCallback<void(bool)>;

  FlocEventLogger(syncer::SyncService* sync_service,
                  FlocRemotePermissionService* floc_remote_permission_service,
                  syncer::UserEventService* user_event_service);

  virtual ~FlocEventLogger();

  // Log a user event. It'll first go though a few permission checks to
  // determine whether the logging is allowed (see class comments). If
  // sync-history is not enabled in particular, it will do a second attempt 10
  // seconds later.
  virtual void LogFlocComputedEvent(Event event);

 private:
  friend class FlocEventLoggerUnitTest;
  friend class MockFlocEventLogger;

  void CheckCanLogEvent(CanLogEventCallback callback);
  void OnCanLogEventDecided(Event event, bool can_log_event);

  bool IsSyncHistoryEnabled() const;

  void IsSwaaNacAccountEnabled(CanLogEventCallback callback);

  // The following raw pointer references are guaranteed to outlive this object.
  syncer::SyncService* sync_service_;
  FlocRemotePermissionService* floc_remote_permission_service_;
  syncer::UserEventService* user_event_service_;

  base::WeakPtrFactory<FlocEventLogger> weak_ptr_factory_{this};
};

}  // namespace federated_learning

#endif  // CHROME_BROWSER_FEDERATED_LEARNING_FLOC_EVENT_LOGGER_H_
