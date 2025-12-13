// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/histograms.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"

namespace cc {

namespace {

// Global data tracking the client name that was set.
// Both of these variables are protected by `GetClientNameLock()`.
static const char* g_client_name = nullptr;
static bool g_multiple_client_names_set = false;

base::Lock& GetClientNameLock() {
  static base::NoDestructor<base::Lock> client_name_lock;
  return *client_name_lock;
}

}  // namespace

void SetClientNameForMetrics(const char* client_name) {
  base::AutoLock auto_lock(GetClientNameLock());

  // Only warn once.
  if (g_multiple_client_names_set)
    return;

  // If a different name is set, return nullptr from now on and log a warning.
  const char* old_client_name = g_client_name;
  if (old_client_name && UNSAFE_TODO(strcmp(old_client_name, client_name))) {
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
  base::AutoLock auto_lock(GetClientNameLock());
  return g_client_name;
}

// Minimum elapsed time of 1us to limit weighting of fast calls.
static const int64_t kMinimumTimeMicroseconds = 1;

ScopedUMAHistogramAreaTimerBase::ScopedUMAHistogramAreaTimerBase() : area_(0) {
}

ScopedUMAHistogramAreaTimerBase::~ScopedUMAHistogramAreaTimerBase() = default;

bool ScopedUMAHistogramAreaTimerBase::GetHistogramValues(
    Sample32* time_microseconds,
    Sample32* pixels_per_ms) const {
  return GetHistogramValues(
      timer_.Elapsed(), area_.ValueOrDefault(std::numeric_limits<int>::max()),
      time_microseconds, pixels_per_ms);
}

// static
bool ScopedUMAHistogramAreaTimerBase::GetHistogramValues(
    base::TimeDelta elapsed,
    int area,
    Sample32* time_microseconds,
    Sample32* pixels_per_ms) {
  elapsed = std::max(elapsed, base::Microseconds(kMinimumTimeMicroseconds));
  double area_per_time = area / elapsed.InMillisecondsF();
  // It is not clear how NaN can get here, but we've gotten crashes from
  // saturated_cast. http://crbug.com/486214
  if (std::isnan(area_per_time))
    return false;
  *time_microseconds = base::saturated_cast<Sample32>(elapsed.InMicroseconds());
  *pixels_per_ms = base::saturated_cast<Sample32>(area_per_time);
  return true;
}

}  // namespace cc
