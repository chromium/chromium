// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_TRIGGER_REFRESH_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_TRIGGER_REFRESH_CHECKER_H_

#include <iosfwd>

#include "base/containers/enum_set.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/sync/base/data_type.h"
#include "components/sync/test/fake_server.h"

namespace syncer {
class SyncService;
class SyncServiceImpl;
}  // namespace syncer

// Checker to block until a refresh sync cycle triggered via
// `SyncService::TriggerRefresh()` for `types` completes (specifically, until
// `FakeServer` processes a `GetUpdates` request with
// `datatype_refresh_nudges() > 0` for the requested active `types` and the
// corresponding sync cycle finishes on the client). The sync engine must
// already be initialized when this checker is constructed.
class TriggerRefreshChecker : public SingleClientStatusChangeChecker,
                              public fake_server::FakeServer::Observer {
 public:
  // `service` and `fake_server` must not be null and must outlive this object.
  // `CommitOnlyTypes()` are automatically excluded from `types` since they are
  // never included in `GetUpdates` requests.
  TriggerRefreshChecker(
      syncer::SyncServiceImpl* service,
      fake_server::FakeServer* fake_server,
      syncer::DataTypeSet types = base::Difference(syncer::DataTypeSet::All(),
                                                   syncer::CommitOnlyTypes()));
  ~TriggerRefreshChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // fake_server::FakeServer::Observer implementation.
  void OnSuccessfulGetUpdates() override;

  // syncer::SyncServiceObserver implementation.
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

 private:
  const raw_ref<fake_server::FakeServer> fake_server_;
  const syncer::DataTypeSet types_;
  base::ScopedObservation<fake_server::FakeServer,
                          fake_server::FakeServer::Observer>
      fake_server_observation_{this};
  bool refresh_get_updates_processed_ = false;
  bool refresh_cycle_completed_ = false;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_TRIGGER_REFRESH_CHECKER_H_
