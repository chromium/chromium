// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_HISTOGRAMS_H_
#define CC_BASE_HISTOGRAMS_H_

#include "base/compiler_specific.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "cc/base/base_export.h"

namespace cc {

// Supplies a client name to be inserted into histogram names.
// These are known so far: Renderer, Browser
//
// We currently assume that there is only one distinct client per process.
// Not thread-safe. If called multiple times, warns and skips metrics.
CC_BASE_EXPORT void SetClientNameForMetrics(const char* client_name);

// Returns the client name, for use by applicable cc metrics code.
// May return null, in which case no clients, or at least two clients, set the
// client name, and these metrics should be omitted.
//
// This method guarantees that it will never return two distinct non-null
// values over the lifetime of the process.
CC_BASE_EXPORT const char* GetClientNameForMetrics();

// Emits UMA histogram trackers for time spent as well as area (in pixels)
// processed per unit time. Time is measured in microseconds, and work in
// pixels per millisecond. Histogram name should include a %s to grab the client
// name.
//
// Usage:
//   // Outside of a method, perhaps in a namespace.
//   DEFINE_SCOPED_UMA_HISTOGRAM_AREA_TIMER(
//       ScopedReticulateSplinesTimer,
//       "Compositing.%s.ReticulateSplinesUs",
//       "Compositing.%s.ReticulateSplinesPixelsPerMs");
//
//   // Inside a method.
//   ScopedReticulateSplinesTimer timer;
//   timer.AddArea(some_rect.size().GetArea());
//
#define DEFINE_SCOPED_UMA_HISTOGRAM_AREA_TIMER(class_name, time_histogram,     \
                                               area_histogram)                 \
  class class_name : public ScopedUMAHistogramAreaTimerBase {                  \
   public:                                                                     \
    ~class_name();                                                             \
  };                                                                           \
  class_name::~class_name() {                                                  \
    Sample time_sample;                                                        \
    Sample area_sample;                                                        \
    const char* client_name = GetClientNameForMetrics();                       \
    if (client_name && GetHistogramValues(&time_sample, &area_sample)) {       \
      /* GetClientNameForMetrics only returns one non-null value over */       \
      /* the lifetime of the process, so these histogram names are */          \
      /* runtime constant. */                                                  \
      UMA_HISTOGRAM_COUNTS_1M(base::StringPrintf(time_histogram, client_name), \
                              time_sample);                                    \
      UMA_HISTOGRAM_CUSTOM_COUNTS(                                             \
          base::StringPrintf(area_histogram, client_name), area_sample, 1,     \
          100000000, 50);                                                      \
    }                                                                          \
  }

// Version of the above macro for cases which only care about time, not area.
#define DEFINE_SCOPED_UMA_HISTOGRAM_TIMER(class_name, time_histogram)          \
  class class_name : public ScopedUMAHistogramAreaTimerBase {                  \
   public:                                                                     \
    ~class_name();                                                             \
  };                                                                           \
  class_name::~class_name() {                                                  \
    Sample time_sample;                                                        \
    Sample area_sample;                                                        \
    const char* client_name = GetClientNameForMetrics();                       \
    if (client_name && GetHistogramValues(&time_sample, &area_sample)) {       \
      DCHECK_EQ(0, area_sample);                                               \
      /* GetClientNameForMetrics only returns one non-null value over */       \
      /* the lifetime of the process, so these histogram names are */          \
      /* runtime constant. */                                                  \
      UMA_HISTOGRAM_COUNTS_1M(base::StringPrintf(time_histogram, client_name), \
                              time_sample);                                    \
    }                                                                          \
  }

class CC_BASE_EXPORT ScopedUMAHistogramAreaTimerBase {
 public:
  ScopedUMAHistogramAreaTimerBase(const ScopedUMAHistogramAreaTimerBase&) =
      delete;
  ScopedUMAHistogramAreaTimerBase& operator=(
      const ScopedUMAHistogramAreaTimerBase&) = delete;

  void AddArea(const base::CheckedNumeric<int>& area) { area_ += area; }
  void SetArea(const base::CheckedNumeric<int>& area) { area_ = area; }

 protected:
  using Sample = base::HistogramBase::Sample;

  ScopedUMAHistogramAreaTimerBase();
  ~ScopedUMAHistogramAreaTimerBase();

  // Returns true if histograms should be recorded (i.e. values are valid).
  bool GetHistogramValues(Sample* time_microseconds,
                          Sample* pixels_per_ms) const;

 private:
  static bool GetHistogramValues(base::TimeDelta elapsed,
                                 int area,
                                 Sample* time_microseconds,
                                 Sample* pixels_per_ms);

  base::ElapsedTimer timer_;
  base::CheckedNumeric<int> area_;

  friend class ScopedUMAHistogramAreaTimerBaseTest;
};

}  // namespace cc

#endif  // CC_BASE_HISTOGRAMS_H_
