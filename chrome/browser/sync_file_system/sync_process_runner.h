// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_PROCESS_RUNNER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_PROCESS_RUNNER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_service_state.h"

namespace sync_file_system {

class SyncFileSystemService;

// A base class to schedule a sync.
// Each subclass must implement StartSync().
// An instance of this class is supposed to be owned by SyncFileSystemService.
//
// Note that multiple SyncProcessRunner doesn't coordinate its sync schedule
// with each other.
class SyncProcessRunner {
 public:
  // Default delay when more changes are available.
  static const int64_t kSyncDelayInMilliseconds;

  // Default delay when the previous change has had an error (but remote service
  // is running).
  static const int64_t kSyncDelayWithSyncError;

  // Default delay when there're more than 10 pending changes.
  static const int64_t kSyncDelayFastInMilliseconds;
  static const int kPendingChangeThresholdForFastSync;

  // Default delay when remote service is temporarily unavailable.
  // The delay backs off exponentially from initial value on repeated failure.
  static const int64_t kSyncDelaySlowInMilliseconds;

  // Default delay when there're no changes.
  static const int64_t kSyncDelayMaxInMilliseconds;

  class Client {
   public:
    virtual ~Client() {}
    virtual void OnSyncIdle() {}
    virtual SyncServiceState GetSyncServiceState() = 0;
    virtual SyncFileSystemService* GetSyncService() = 0;
  };

  class TimerHelper {
   public:
    virtual ~TimerHelper() {}
    virtual bool IsRunning() = 0;
    virtual void Start(const base::Location& from_here,
                       const base::TimeDelta& delay,
                       const base::Closure& closure) = 0;
    virtual base::TimeTicks Now() const = 0;

   protected:
    TimerHelper() {}
  };

  SyncProcessRunner(const std::string& name,
                    Client* client,
                    std::unique_ptr<TimerHelper> timer_helper,
                    size_t max_parallel_task);
  virtual ~SyncProcessRunner();

  // Subclass must implement this.
  virtual void StartSync(const SyncStatusCallback& callback) = 0;

  // Schedules a new sync.
  void Schedule();

  int64_t pending_changes() const { return pending_changes_; }

  // Returns the current service state.  Default implementation returns
  // sync_service()->GetSyncServiceState().
  virtual SyncServiceState GetServiceState();

 protected:
  void OnChangesUpdated(int64_t pending_changes);
  SyncFileSystemService* GetSyncService();

 private:
  void Finished(const base::TimeTicks& start_time, SyncStatusCode status);
  void Run();
  void ScheduleInternal(int64_t delay);

  // Throttles new sync for |base_delay| milliseconds for an error case.
  // If new sync is already throttled, back off the duration.
  void ThrottleSync(int64_t base_delay);

  // Clears old throttling setting that is already over.
  void ResetOldThrottling();
  void ResetThrottling();

  void CheckIfIdle();

  std::string name_;
  Client* client_;
  size_t max_parallel_task_;
  size_t running_tasks_;
  std::unique_ptr<TimerHelper> timer_helper_;
  base::TimeTicks last_run_;
  base::TimeTicks last_scheduled_;
  SyncServiceState service_state_;

  base::TimeTicks throttle_from_;
  base::TimeTicks throttle_until_;

  int64_t pending_changes_;
  base::WeakPtrFactory<SyncProcessRunner> factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncProcessRunner);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_PROCESS_RUNNER_H_
