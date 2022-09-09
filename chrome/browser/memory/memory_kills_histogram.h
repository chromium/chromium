// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_KILLS_HISTOGRAM_H_
#define CHROME_BROWSER_MEMORY_MEMORY_KILLS_HISTOGRAM_H_

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

namespace memory {

constexpr base::TimeDelta kMaxMemoryKillTimeDelta = base::Seconds(30);

}  // namespace memory

// Use this macro to report elapsed time since last Memory kill event.
// Must be a macro as the underlying HISTOGRAM macro creates static variables.
#define UMA_HISTOGRAM_MEMORY_KILL_TIME_INTERVAL(name, sample)     \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, base::Milliseconds(1), \
                             ::memory::kMaxMemoryKillTimeDelta, 50)

#endif  // CHROME_BROWSER_MEMORY_MEMORY_KILLS_HISTOGRAM_H_
