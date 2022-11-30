// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/scheduling/fake_nearby_share_scheduler.h"

#include <utility>

FakeNearbyShareScheduler::FakeNearbyShareScheduler(OnRequestCallback callback)
    : NearbyShareScheduler(std::move(callback)) {}

FakeNearbyShareScheduler::~FakeNearbyShareScheduler() = default;

void FakeNearbyShareScheduler::MakeImmediateRequest() {
  ++num_immediate_requests_;
}

void FakeNearbyShareScheduler::HandleResult(bool success) {
  handled_results_.push_back(success);
}

void FakeNearbyShareScheduler::Reschedule() {
  ++num_reschedule_calls_;
}

absl::optional<base::Time> FakeNearbyShareScheduler::GetLastSuccessTime()
    const {
  return last_success_time_;
}

absl::optional<base::TimeDelta>
FakeNearbyShareScheduler::GetTimeUntilNextRequest() const {
  return time_until_next_request_;
}

bool FakeNearbyShareScheduler::IsWaitingForResult() const {
  return is_waiting_for_result_;
}

size_t FakeNearbyShareScheduler::GetNumConsecutiveFailures() const {
  return num_consecutive_failures_;
}

void FakeNearbyShareScheduler::OnStart() {
  can_invoke_request_callback_ = true;
}

void FakeNearbyShareScheduler::OnStop() {
  can_invoke_request_callback_ = false;
}

void FakeNearbyShareScheduler::InvokeRequestCallback() {
  DCHECK(can_invoke_request_callback_);
  NotifyOfRequest();
}

void FakeNearbyShareScheduler::SetLastSuccessTime(
    absl::optional<base::Time> time) {
  last_success_time_ = time;
}

void FakeNearbyShareScheduler::SetTimeUntilNextRequest(
    absl::optional<base::TimeDelta> time_delta) {
  time_until_next_request_ = time_delta;
}

void FakeNearbyShareScheduler::SetIsWaitingForResult(bool is_waiting) {
  is_waiting_for_result_ = is_waiting;
}

void FakeNearbyShareScheduler::SetNumConsecutiveFailures(size_t num_failures) {
  num_consecutive_failures_ = num_failures;
}
