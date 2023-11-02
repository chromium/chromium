// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_SCHEDULER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_SCHEDULER_H_

namespace offline_pages {

class PrefetchBackgroundTaskScheduler {
 public:
  // API for interacting with BackgroundTaskScheduler from native.
  // Schedules the default 'NWake' prefetching task.
  // |additional_delay_seconds| is relative to the default 15 minute delay.
  // Implemented in platform-specific object files.
  static void Schedule(int additional_delay_seconds);

  // Same as |Schedule| but adapted to when limitless prefetching is enabled so
  // that less restrictions are applied to the scheduling of the background
  // task.
  static void ScheduleLimitless(int additional_delay_seconds);

  // Cancels the default 'NWake' prefetching task.
  // Implemented in platform-specific object files.
  static void Cancel();
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_SCHEDULER_H_
