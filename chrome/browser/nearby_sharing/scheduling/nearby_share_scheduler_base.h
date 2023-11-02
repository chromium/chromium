// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_BASE_H_
#define CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_BASE_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Clock;
}  // namespace base

class PrefService;

// A base NearbyShareScheduler implementation that persists scheduling data.
// Requests made before scheduling has started, while another attempt is in
// progress, or while offline are cached and rescheduled as soon as possible.
// Likewise, when the scheduler is stopped or destroyed, scheduling data is
// persisted and restored when the scheduler is restarted or recreated,
// respectively.
//
// If automatic failure retry is enabled, all failed attempts follow an
// exponential backoff retry strategy.
//
// The scheduler waits until the device is online before notifying the owner if
// network connectivity is required.
//
// Derived classes must override TimeUntilRecurringRequest() to establish the
// desired recurring request behavior of the scheduler.
class NearbyShareSchedulerBase
    : public NearbyShareScheduler,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  ~NearbyShareSchedulerBase() override;

 protected:
  // |retry_failures|: Whether or not automatically retry failures using
  //     exponential backoff strategy.
  // |require_connectivity|: If true, the scheduler will not alert the owner of
  //     a request until network connectivity is established.
  // |pref_name|: The dictionary pref name used to persist scheduling data. Make
  //     sure to register this pref name before creating the scheduler.
  // |pref_service|: The pref service used to persist scheduling data.
  // |callback|: The function invoked to alert the owner that a request is due.
  // |clock|: The clock used to determine timer delays.
  NearbyShareSchedulerBase(bool retry_failures,
                           bool require_connectivity,
                           const std::string& pref_name,
                           PrefService* pref_service,
                           OnRequestCallback callback,
                           const base::Clock* clock);

  // The time to wait until the next regularly recurring request.
  virtual absl::optional<base::TimeDelta> TimeUntilRecurringRequest(
      base::Time now) const = 0;

  // NearbyShareScheduler:
  void MakeImmediateRequest() override;
  void HandleResult(bool success) override;
  void Reschedule() override;
  absl::optional<base::Time> GetLastSuccessTime() const override;
  absl::optional<base::TimeDelta> GetTimeUntilNextRequest() const override;
  bool IsWaitingForResult() const override;
  size_t GetNumConsecutiveFailures() const override;
  void OnStart() override;
  void OnStop() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  absl::optional<base::Time> GetLastAttemptTime() const;
  bool HasPendingImmediateRequest() const;

  // Set and persist scheduling data in prefs.
  void SetLastAttemptTime(base::Time last_attempt_time);
  void SetLastSuccessTime(base::Time last_success_time);
  void SetNumConsecutiveFailures(size_t num_failures);
  void SetHasPendingImmediateRequest(bool has_pending_immediate_request);
  void SetIsWaitingForResult(bool is_waiting_for_result);

  // On startup, set a pending immediate request if the pref service indicates
  // that there was an in-progress request or a pending immediate request at the
  // time of shutdown.
  void InitializePersistedRequest();

  // The amount of time to wait until the next automatic failure retry. Returns
  // absl::nullopt if there is no failure to retry or if failure retry is not
  // enabled for the scheduler.
  absl::optional<base::TimeDelta> TimeUntilRetry(base::Time now) const;

  // Notifies the owner that a request is ready. Early returns if not online and
  // the scheduler requires connectivity; the attempt is rescheduled when
  // connectivity is restored.
  void OnTimerFired();

  void PrintSchedulerState() const;

  bool retry_failures_;
  bool require_connectivity_;
  std::string pref_name_;
  PrefService* pref_service_ = nullptr;
  const base::Clock* clock_ = nullptr;
  base::OneShotTimer timer_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_BASE_H_
