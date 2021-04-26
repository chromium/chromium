// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_STARTUP_TRACKER_H_
#define CHROME_BROWSER_SYNC_SYNC_STARTUP_TRACKER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/driver/sync_service_observer.h"

// SyncStartupTracker provides a centralized way for observers to detect when
// SyncService has successfully started up, or when startup has failed
// due to some kind of error. This code was originally part of SigninTracker
// but now that sync initialization is no longer a required part of signin,
// it has been broken out of that class so only those places that care about
// sync initialization depend on it.
class SyncStartupTracker : public syncer::SyncServiceObserver {
 public:
  // Observer interface used to notify observers when sync has started up.
  class Observer {
   public:
    virtual ~Observer() {}

    virtual void SyncStartupCompleted() = 0;
    virtual void SyncStartupFailed() = 0;
  };

  SyncStartupTracker(syncer::SyncService* sync_service, Observer* observer);
  ~SyncStartupTracker() override;

  enum SyncServiceState {
    // Sync backend is still starting up.
    SYNC_STARTUP_PENDING,
    // An error has been detected that prevents the sync backend from starting
    // up.
    SYNC_STARTUP_ERROR,
    // Sync startup has completed (i.e. SyncService::IsEngineInitialized()
    // returns true).
    SYNC_STARTUP_COMPLETE
  };

  // Returns the current state of the sync service.
  static SyncServiceState GetSyncServiceState(
      syncer::SyncService* sync_service);

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  // Checks the current service state and notifies |observer_| if the state
  // has changed. Note that it is expected that the observer will free this
  // object, so callers should not reference this object after making this call.
  void CheckServiceState();

  // The SyncService we should track.
  syncer::SyncService* sync_service_;

  // Weak pointer to the observer to notify.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(SyncStartupTracker);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_STARTUP_TRACKER_H_
