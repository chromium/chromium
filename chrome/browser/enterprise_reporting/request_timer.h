// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REQUEST_TIMER_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REQUEST_TIMER_H_

#include <memory>

#include "base/timer/timer.h"

namespace enterprise_reporting {

// A Retaining timer which runs the same task with same delay after every reset
// except the first one which has a special delay. This is used by
// ReportScheduler to schedule the next report.
class RequestTimer {
 public:
  RequestTimer();
  virtual ~RequestTimer();

  // Starts the timer. The first task will be ran after |first_delay|. The
  // following task will be ran with |repeat_delay|. If |first_delay| is larger
  // than the |repeat_delay|, the first request will be fired after
  // |repeat_delay| instead. Also, please note that the repeating task is ran
  // once per Reset call.
  virtual void Start(const base::Location& posted_from,
                     base::TimeDelta first_delay,
                     base::TimeDelta repeat_delay,
                     base::RepeatingClosure user_task);
  // Stops the timer. The running task will not be abandon.
  virtual void Stop();
  // Resets the timer, ran the task again after |repat_delay| that is set in
  // Start(); This is only available after the first task is ran.
  virtual void Reset();

  bool IsRepeatTimerRunning() const;
  bool IsFirstTimerRunning() const;

 private:
  base::OneShotTimer first_request_timer_;
  std::unique_ptr<base::RetainingOneShotTimer> repeat_request_timer_;

  DISALLOW_COPY_AND_ASSIGN(RequestTimer);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REQUEST_TIMER_H_
