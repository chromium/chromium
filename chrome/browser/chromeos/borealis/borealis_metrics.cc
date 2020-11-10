// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_metrics.h"

namespace borealis {

const char kBorealisStartupNumAttemptsHistogram[] =
    "Borealis.Startup.NumAttempts";
const char kBorealisStartupResultHistogram[] = "Borealis.Startup.Result";
const char kBorealisStartupOverallTimeHistogram[] =
    "Borealis.Startup.OverallTime";

void RecordBorealisStartupNumAttemptsHistogram() {
  base::UmaHistogramBoolean(kBorealisStartupNumAttemptsHistogram, true);
}

void RecordBorealisStartupResultHistogram(
    BorealisStartupResult startup_result) {
  base::UmaHistogramEnumeration(kBorealisStartupResultHistogram,
                                startup_result);
}

void RecordBorealisStartupOverallTimeHistogram(base::TimeDelta startup_time) {
  base::UmaHistogramTimes(kBorealisStartupOverallTimeHistogram, startup_time);
}

}  // namespace borealis
