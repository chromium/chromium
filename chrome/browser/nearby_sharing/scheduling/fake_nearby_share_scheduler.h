// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_H_
#define CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_H_

#include <vector>

#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// A fake implementation of NearbyShareScheduler that allows the user to set all
// scheduling data. It tracks the number of immediate requests and the handled
// results. The on-request callback can be invoked using
// InvokeRequestCallback().
class FakeNearbyShareScheduler : public NearbyShareScheduler {
 public:
  explicit FakeNearbyShareScheduler(OnRequestCallback callback);
  ~FakeNearbyShareScheduler() override;

  // NearbyShareScheduler:
  void MakeImmediateRequest() override;
  void HandleResult(bool success) override;
  void Reschedule() override;
  absl::optional<base::Time> GetLastSuccessTime() const override;
  absl::optional<base::TimeDelta> GetTimeUntilNextRequest() const override;
  bool IsWaitingForResult() const override;
  size_t GetNumConsecutiveFailures() const override;

  void SetLastSuccessTime(absl::optional<base::Time> time);
  void SetTimeUntilNextRequest(absl::optional<base::TimeDelta> time_delta);
  void SetIsWaitingForResult(bool is_waiting);
  void SetNumConsecutiveFailures(size_t num_failures);

  void InvokeRequestCallback();

  size_t num_immediate_requests() const { return num_immediate_requests_; }
  size_t num_reschedule_calls() const { return num_reschedule_calls_; }
  const std::vector<bool>& handled_results() const { return handled_results_; }

 private:
  // NearbyShareScheduler:
  void OnStart() override;
  void OnStop() override;

  bool can_invoke_request_callback_ = false;
  size_t num_immediate_requests_ = 0;
  size_t num_reschedule_calls_ = 0;
  std::vector<bool> handled_results_;
  absl::optional<base::Time> last_success_time_;
  absl::optional<base::TimeDelta> time_until_next_request_;
  bool is_waiting_for_result_ = false;
  size_t num_consecutive_failures_ = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_H_
