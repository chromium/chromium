// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_process_runner.h"

#include <utility>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "chrome/browser/sync_file_system/logger.h"

namespace sync_file_system {

const int64_t SyncProcessRunner::kSyncDelayInMilliseconds =
    1 * base::Time::kMillisecondsPerSecond;  // 1 sec
const int64_t SyncProcessRunner::kSyncDelayWithSyncError =
    3 * base::Time::kMillisecondsPerSecond;                           // 3 sec
const int64_t SyncProcessRunner::kSyncDelayFastInMilliseconds = 100;  // 100 ms
const int SyncProcessRunner::kPendingChangeThresholdForFastSync = 10;
const int64_t SyncProcessRunner::kSyncDelaySlowInMilliseconds =
    30 * base::Time::kMillisecondsPerSecond;  // 30 sec
const int64_t SyncProcessRunner::kSyncDelayMaxInMilliseconds =
    30 * 60 * base::Time::kMillisecondsPerSecond;  // 30 min

namespace {

class BaseTimerHelper : public SyncProcessRunner::TimerHelper {
 public:
  BaseTimerHelper() {}

  bool IsRunning() override { return timer_.IsRunning(); }

  void Start(const base::Location& from_here,
             const base::TimeDelta& delay,
             const base::Closure& closure) override {
    timer_.Start(from_here, delay, closure);
  }

  base::TimeTicks Now() const override { return base::TimeTicks::Now(); }

  ~BaseTimerHelper() override {}

 private:
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(BaseTimerHelper);
};

bool WasSuccessfulSync(SyncStatusCode status) {
  return status == SYNC_STATUS_OK ||
         status == SYNC_STATUS_HAS_CONFLICT ||
         status == SYNC_STATUS_NO_CONFLICT ||
         status == SYNC_STATUS_NO_CHANGE_TO_SYNC ||
         status == SYNC_STATUS_UNKNOWN_ORIGIN ||
         status == SYNC_STATUS_RETRY;
}

}  // namespace

SyncProcessRunner::SyncProcessRunner(const std::string& name,
                                     Client* client,
                                     std::unique_ptr<TimerHelper> timer_helper,
                                     size_t max_parallel_task)
    : name_(name),
      client_(client),
      max_parallel_task_(max_parallel_task),
      running_tasks_(0),
      timer_helper_(std::move(timer_helper)),
      service_state_(SYNC_SERVICE_RUNNING),
      pending_changes_(0) {
  DCHECK_LE(1u, max_parallel_task_);
  if (!timer_helper_)
    timer_helper_.reset(new BaseTimerHelper);
}

SyncProcessRunner::~SyncProcessRunner() {}

void SyncProcessRunner::Schedule() {
  if (pending_changes_ == 0) {
    ScheduleInternal(kSyncDelayMaxInMilliseconds);
    return;
  }

  SyncServiceState last_service_state = service_state_;
  service_state_ = GetServiceState();

  switch (service_state_) {
    case SYNC_SERVICE_RUNNING:
      ResetThrottling();
      if (pending_changes_ > kPendingChangeThresholdForFastSync)
        ScheduleInternal(kSyncDelayFastInMilliseconds);
      else
        ScheduleInternal(kSyncDelayInMilliseconds);
      return;

    case SYNC_SERVICE_TEMPORARY_UNAVAILABLE:
      if (last_service_state != service_state_)
        ThrottleSync(kSyncDelaySlowInMilliseconds);
      ScheduleInternal(kSyncDelaySlowInMilliseconds);
      return;

    case SYNC_SERVICE_AUTHENTICATION_REQUIRED:
    case SYNC_SERVICE_DISABLED:
      if (last_service_state != service_state_)
        ThrottleSync(kSyncDelaySlowInMilliseconds);
      ScheduleInternal(kSyncDelayMaxInMilliseconds);
      return;
  }

  NOTREACHED();
  ScheduleInternal(kSyncDelayMaxInMilliseconds);
}

void SyncProcessRunner::ThrottleSync(int64_t base_delay) {
  base::TimeTicks now = timer_helper_->Now();
  base::TimeDelta elapsed = std::min(now, throttle_until_) - throttle_from_;
  DCHECK(base::TimeDelta() <= elapsed);

  throttle_from_ = now;
  // Extend throttling duration by twice the elapsed time.
  // That is, if the backoff repeats in a short period, the throttling period
  // doesn't grow exponentially.  If the backoff happens on the end of
  // throttling period, it causes another throttling period that is twice as
  // long as previous.
  base::TimeDelta base_delay_delta =
      base::TimeDelta::FromMilliseconds(base_delay);
  const base::TimeDelta max_delay =
      base::TimeDelta::FromMilliseconds(kSyncDelayMaxInMilliseconds);
  throttle_until_ =
      std::min(now + max_delay,
               std::max(now + base_delay_delta, throttle_until_ + 2 * elapsed));
}

void SyncProcessRunner::ResetOldThrottling() {
  if (throttle_until_ < base::TimeTicks::Now())
    ResetThrottling();
}

void SyncProcessRunner::ResetThrottling() {
  throttle_from_ = base::TimeTicks();
  throttle_until_ = base::TimeTicks();
}

SyncServiceState SyncProcessRunner::GetServiceState() {
  return client_->GetSyncServiceState();
}

void SyncProcessRunner::OnChangesUpdated(int64_t pending_changes) {
  DCHECK_GE(pending_changes, 0);
  int64_t old_pending_changes = pending_changes_;
  pending_changes_ = pending_changes;
  if (old_pending_changes != pending_changes) {
    CheckIfIdle();
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[%s] pending_changes updated: %" PRId64,
              name_.c_str(), pending_changes);
  }
  Schedule();
}

SyncFileSystemService* SyncProcessRunner::GetSyncService() {
  return client_->GetSyncService();
}

void SyncProcessRunner::Finished(const base::TimeTicks& start_time,
                                 SyncStatusCode status) {
  DCHECK_LT(0u, running_tasks_);
  DCHECK_LE(running_tasks_, max_parallel_task_);
  --running_tasks_;
  CheckIfIdle();
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[%s] * Finished (elapsed: %" PRId64 " ms)", name_.c_str(),
            (timer_helper_->Now() - start_time).InMilliseconds());

  if (status == SYNC_STATUS_NO_CHANGE_TO_SYNC ||
      status == SYNC_STATUS_FILE_BUSY) {
    ScheduleInternal(kSyncDelayMaxInMilliseconds);
    return;
  }

  if (WasSuccessfulSync(status))
    ResetOldThrottling();
  else
    ThrottleSync(kSyncDelayWithSyncError);

  Schedule();
}

void SyncProcessRunner::Run() {
  if (running_tasks_ >= max_parallel_task_)
    return;
  ++running_tasks_;
  base::TimeTicks now = timer_helper_->Now();
  last_run_ = now;

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[%s] * Started", name_.c_str());

  StartSync(base::Bind(&SyncProcessRunner::Finished, factory_.GetWeakPtr(),
                       now));
  if (running_tasks_ < max_parallel_task_)
    Schedule();
}

void SyncProcessRunner::ScheduleInternal(int64_t delay) {
  base::TimeTicks now = timer_helper_->Now();
  base::TimeTicks next_scheduled;

  if (timer_helper_->IsRunning()) {
    next_scheduled = last_run_ + base::TimeDelta::FromMilliseconds(delay);
    if (next_scheduled < now) {
      next_scheduled =
          now + base::TimeDelta::FromMilliseconds(kSyncDelayFastInMilliseconds);
    }
  } else {
    next_scheduled = now + base::TimeDelta::FromMilliseconds(delay);
  }

  if (next_scheduled < throttle_until_)
    next_scheduled = throttle_until_;

  if (timer_helper_->IsRunning() && last_scheduled_ == next_scheduled)
    return;

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[%s] Scheduling task in %" PRId64 " ms",
            name_.c_str(), (next_scheduled - now).InMilliseconds());

  last_scheduled_ = next_scheduled;

  timer_helper_->Start(
      FROM_HERE, next_scheduled - now,
      base::Bind(&SyncProcessRunner::Run, base::Unretained(this)));
}

void SyncProcessRunner::CheckIfIdle() {
  if (pending_changes_ == 0 && running_tasks_ == 0)
    client_->OnSyncIdle();
}

}  // namespace sync_file_system
