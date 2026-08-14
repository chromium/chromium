// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_OS_REPORTER_H_
#define CC_METRICS_SCROLL_JANK_OS_REPORTER_H_

#include <stdint.h>

#include "cc/cc_export.h"

namespace cc {

// Abstract interface used by `ScrollJankV4HistogramEmitter` to report scroll
// jank statistics to the OS.
class CC_EXPORT ScrollJankOsReporter {
 public:
  virtual ~ScrollJankOsReporter() = default;

  // Reports scroll jank statistics to the OS.
  //
  // `janky_frames` must be less than or equal to `total_frames`.
  virtual void ReportScrollJankStats(uint32_t total_frames,
                                     uint32_t janky_frames) = 0;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_OS_REPORTER_H_
