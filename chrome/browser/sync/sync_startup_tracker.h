// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_STARTUP_TRACKER_H_
#define CHROME_BROWSER_SYNC_SYNC_STARTUP_TRACKER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

// `SyncStartupTracker` provides an easier way to wait for `SyncService` to be
// successfully started up, or to be notified when startup has failed due to
// some kind of error.
class SyncStartupTracker : public syncer::SyncServiceObserver {
 public:
  enum class ServiceStartupState {
    // Sync backend is still starting up.
    kPending,
    // An error has been detected that prevents the sync backend from starting
    // up.
    kError,
    // Sync startup has completed (i.e. `SyncService::IsEngineInitialized()`
    // returns true).
    kComplete,
    // Sync startup is taking too long. This can only be obtained when waiting
    // for startup via `SyncStartupTracker`.
    kTimeout,
  };

  using SyncStartupStateChangedCallback =
      base::OnceCallback<void(ServiceStartupState)>;

  // Starts observing the status of `sync_service` and runs `callback` when its
  // startup completes (or fails). If the tracker is destroyed before `callback`
  // is run, it will just be dropped without running.
  SyncStartupTracker(syncer::SyncService* sync_service,
                     SyncStartupStateChangedCallback callback);

  SyncStartupTracker(const SyncStartupTracker&) = delete;
  SyncStartupTracker& operator=(const SyncStartupTracker&) = delete;

  ~SyncStartupTracker() override;

  // Returns the current state of the sync service.
  static ServiceStartupState GetServiceStartupState(
      syncer::SyncService* sync_service);

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  // Checks the current service state and notifies the
  // `sync_startup_status_changed_callback_` if the state has changed. Note that
  // it is expected that the observer will free this object, so callers should
  // not reference this object after making this call.
  void CheckServiceState();

  void OnStartupTimeout();

  // The SyncService we should track.
  const raw_ptr<syncer::SyncService> sync_service_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  base::OneShotTimer timeout_waiter_;
  bool is_timed_out_ = false;

  SyncStartupStateChangedCallback sync_startup_status_changed_callback_;

  base::WeakPtrFactory<SyncStartupTracker> weak_factory_{this};
};

namespace testing {
// Helper to control `SyncStartupTracker`'s timeout mechanism for tests. If it
// is created with an empty timeout value, it will make the tracker not report
// timeouts.
class ScopedSyncStartupTimeoutOverride {
 public:
  explicit ScopedSyncStartupTimeoutOverride(
      std::optional<base::TimeDelta> wait_timeout);
  ~ScopedSyncStartupTimeoutOverride();

 private:
  std::optional<base::TimeDelta> old_wait_timeout_;
};
}  // namespace testing

#endif  // CHROME_BROWSER_SYNC_SYNC_STARTUP_TRACKER_H_
