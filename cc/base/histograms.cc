// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/histograms.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"

namespace cc {

// Global data tracking the client name that was set.
// Both of these variables are protected by the lock.
static base::LazyInstance<base::Lock>::Leaky g_client_name_lock =
    LAZY_INSTANCE_INITIALIZER;
static const char* g_client_name = nullptr;
static bool g_multiple_client_names_set = false;

void SetClientNameForMetrics(const char* client_name) {
  base::AutoLock auto_lock(g_client_name_lock.Get());

  // Only warn once.
  if (g_multiple_client_names_set)
    return;

  // If a different name is set, return nullptr from now on and log a warning.
  const char* old_client_name = g_client_name;
  if (old_client_name && strcmp(old_client_name, client_name)) {
    g_client_name = nullptr;
    g_multiple_client_names_set = true;
    LOG(WARNING) << "Started multiple compositor clients (" << old_client_name
                 << ", " << client_name
                 << ") in one process. Some metrics will be disabled.";
    return;
  }

  // If the client name is being set for the first time, store it.
  if (!old_client_name)
    g_client_name = client_name;
}

const char* GetClientNameForMetrics() {
  base::AutoLock auto_lock(g_client_name_lock.Get());
  return g_client_name;
}

// Minimum elapsed time of 1us to limit weighting of fast calls.
static const int64_t kMinimumTimeMicroseconds = 1;

ScopedUMAHistogramAreaTimerBase::ScopedUMAHistogramAreaTimerBase() : area_(0) {
}

ScopedUMAHistogramAreaTimerBase::~ScopedUMAHistogramAreaTimerBase() = default;

bool ScopedUMAHistogramAreaTimerBase::GetHistogramValues(
    Sample* time_microseconds,
    Sample* pixels_per_ms) const {
  return GetHistogramValues(
      timer_.Elapsed(), area_.ValueOrDefault(std::numeric_limits<int>::max()),
      time_microseconds, pixels_per_ms);
}

// static
bool ScopedUMAHistogramAreaTimerBase::GetHistogramValues(
    base::TimeDelta elapsed,
    int area,
    Sample* time_microseconds,
    Sample* pixels_per_ms) {
  elapsed = std::max(elapsed, base::Microseconds(kMinimumTimeMicroseconds));
  double area_per_time = area / elapsed.InMillisecondsF();
  // It is not clear how NaN can get here, but we've gotten crashes from
  // saturated_cast. http://crbug.com/486214
  if (std::isnan(area_per_time))
    return false;
  *time_microseconds = base::saturated_cast<Sample>(elapsed.InMicroseconds());
  *pixels_per_ms = base::saturated_cast<Sample>(area_per_time);
  return true;
}

}  // namespace cc
