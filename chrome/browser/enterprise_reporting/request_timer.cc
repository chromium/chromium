// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/request_timer.h"

#include "base/time/time.h"

namespace enterprise_reporting {

RequestTimer::RequestTimer() = default;
RequestTimer::~RequestTimer() = default;

void RequestTimer::Start(const base::Location& posted_from,
                         base::TimeDelta first_delay,
                         base::TimeDelta repeat_delay,
                         base::RepeatingClosure user_task) {
  // Create |repeat_request_timer| but not start it because the first request is
  // sent after |first_delay|.
  repeat_request_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, repeat_delay, user_task);

  if (first_delay >= repeat_delay) {
    repeat_request_timer_->Reset();
  } else {
    first_request_timer_.Start(FROM_HERE, first_delay,
                               base::BindOnce(user_task));
  }
}

void RequestTimer::Stop() {
  if (repeat_request_timer_)
    repeat_request_timer_->Stop();
  first_request_timer_.Stop();
}

void RequestTimer::Reset() {
  DCHECK(!first_request_timer_.IsRunning());
  DCHECK(repeat_request_timer_);
  repeat_request_timer_->Reset();
}

bool RequestTimer::IsRepeatTimerRunning() const {
  return repeat_request_timer_ && repeat_request_timer_->IsRunning();
}

bool RequestTimer::IsFirstTimerRunning() const {
  return first_request_timer_.IsRunning();
}

}  // namespace enterprise_reporting
