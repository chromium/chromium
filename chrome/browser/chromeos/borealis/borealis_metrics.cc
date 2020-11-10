// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace borealis {

const char kBorealisInstallNumAttemptsHistogram[] =
    "Borealis.Install.NumAttempts";
const char kBorealisInstallResultHistogram[] = "Borealis.Install.Result";
const char kBorealisInstallOverallTimeHistogram[] =
    "Borealis.Install.OverallTime";
const char kBorealisStartupNumAttemptsHistogram[] =
    "Borealis.Startup.NumAttempts";
const char kBorealisStartupResultHistogram[] = "Borealis.Startup.Result";
const char kBorealisStartupOverallTimeHistogram[] =
    "Borealis.Startup.OverallTime";

void RecordBorealisInstallNumAttemptsHistogram() {
  base::UmaHistogramBoolean(kBorealisInstallNumAttemptsHistogram, true);
}

void RecordBorealisInstallResultHistogram(
    BorealisInstallResult install_result) {
  base::UmaHistogramEnumeration(kBorealisInstallResultHistogram,
                                install_result);
}

void RecordBorealisInstallOverallTimeHistogram(base::TimeDelta install_time) {
  base::UmaHistogramTimes(kBorealisInstallOverallTimeHistogram, install_time);
}

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

std::ostream& operator<<(std::ostream& stream,
                         borealis::BorealisStartupResult result) {
  switch (result) {
    case borealis::BorealisStartupResult::kSuccess:
      return stream << "Success";
    case borealis::BorealisStartupResult::kCancelled:
      return stream << "Cancelled";
    case borealis::BorealisStartupResult::kMountFailed:
      return stream << "Mount Failed";
    case borealis::BorealisStartupResult::kDiskImageFailed:
      return stream << "Disk Image Failed";
    case borealis::BorealisStartupResult::kStartVmFailed:
      return stream << "Start VM Failed";
    case borealis::BorealisStartupResult::kAwaitBorealisStartupFailed:
      return stream << "Await Borealis Startup Failed";
  }
}
