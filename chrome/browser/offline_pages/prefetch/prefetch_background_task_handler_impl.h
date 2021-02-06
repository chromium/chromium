// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_HANDLER_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_HANDLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"

class PrefService;

namespace base {
class TickClock;
}

namespace net {
class BackoffEntry;
}

namespace offline_pages {

// A class living on the PrefetchService that is able to ask the Android
// background task system to schedule tasks at the appropriate time based on the
// backoff value, or cancel tasks.
//
// The background task will be rescheduled in the time calculated as:
//    delay = 15 minutes +  backoff
//    backoff = (30 seconds ^ N * Rand(0.67, 1)) | (1 day)
// 15 minutes is the minimum delay, defined in PrefetchBackgroundTaskScheduler.
// |backoff| is the additional delay needed to avoid overflowing the server,
// which could be one of the following:
// 1) 0: No additional delay when the request succeeds
// 2) 30s^N * Rand(0.67,1): Exponential backoff w/ jitter when the request fails
//                          (N is the count of the request failures)
// 3) 1 day: Suspend for 1 day due to certain severe server errors
// The backoff value is controlled by a persisted BackoffEntry.
class PrefetchBackgroundTaskHandlerImpl : public PrefetchBackgroundTaskHandler {
 public:
  explicit PrefetchBackgroundTaskHandlerImpl(PrefService* profile);
  ~PrefetchBackgroundTaskHandlerImpl() override;

  // PrefetchBackgroundTaskHandler implementation.
  void CancelBackgroundTask() override;
  void EnsureTaskScheduled() override;

  // Backoff control.  These functions directly modify/read prefs.
  void Backoff() override;
  void ResetBackoff() override;
  void PauseBackoffUntilNextRun() override;
  void Suspend() override;
  void RemoveSuspension() override;
  int GetAdditionalBackoffSeconds() const override;

  // This is used to construct the backoff value.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

 private:
  std::unique_ptr<net::BackoffEntry> GetCurrentBackoff() const;
  void UpdateBackoff(net::BackoffEntry* backoff);

  PrefService* prefs_;
  const base::TickClock* tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchBackgroundTaskHandlerImpl);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_HANDLER_IMPL_H_
