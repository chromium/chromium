// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_METRICS_H_

#include "base/metrics/histogram_functions.h"

namespace borealis {

extern const char kBorealisStartupNumAttemptsHistogram[];
extern const char kBorealisStartupResultHistogram[];
extern const char kBorealisStartupOverallTimeHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BorealisStartupResult {
  kSuccess = 0,
  kCancelled = 1,
  kMountFailed = 2,
  kDiskImageFailed = 3,
  kStartVmFailed = 4,
  kAwaitBorealisStartupFailed = 5,
  kMaxValue = kAwaitBorealisStartupFailed,
};

void RecordBorealisStartupNumAttemptsHistogram();
void RecordBorealisStartupResultHistogram(BorealisStartupResult startup_result);
void RecordBorealisStartupOverallTimeHistogram(base::TimeDelta startup_time);

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_METRICS_H_
