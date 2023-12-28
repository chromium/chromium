// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_MULTI_CLIENT_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_MULTI_CLIENT_STATUS_CHANGE_CHECKER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_service_observer.h"

// This class provides some common functionality for StatusChangeCheckers that
// observe many SyncServiceImpls.  This class is abstract.  Its descendants
// are expected to provide additional functionality.
class MultiClientStatusChangeChecker : public StatusChangeChecker,
                                       public syncer::SyncServiceObserver {
 public:
  explicit MultiClientStatusChangeChecker(
      std::vector<raw_ptr<syncer::SyncServiceImpl, VectorExperimental>>
          services);

  MultiClientStatusChangeChecker(const MultiClientStatusChangeChecker&) =
      delete;
  MultiClientStatusChangeChecker& operator=(
      const MultiClientStatusChangeChecker&) = delete;

  ~MultiClientStatusChangeChecker() override;

 protected:
  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // StatusChangeChecker implementations and stubs.
  bool IsExitConditionSatisfied(std::ostream* os) override = 0;

  const std::vector<raw_ptr<syncer::SyncServiceImpl, VectorExperimental>>&
  services() {
    return services_;
  }

 private:
  std::vector<raw_ptr<syncer::SyncServiceImpl, VectorExperimental>> services_;
  base::ScopedMultiSourceObservation<syncer::SyncService,
                                     syncer::SyncServiceObserver>
      scoped_observations_{this};
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_MULTI_CLIENT_STATUS_CHANGE_CHECKER_H_
