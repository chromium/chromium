// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_types.mojom.h"

namespace borealis {

using borealis::mojom::InstallResult;

const char kBorealisInstallNumAttemptsHistogram[] =
    "Borealis.Install.NumAttempts";
const char kBorealisInstallResultHistogram[] = "Borealis.Install.Result";
const char kBorealisInstallOverallTimeHistogram[] =
    "Borealis.Install.OverallTime";
// Same as Borealis.Install.OverallTime, but with more appropriate bucket sizes.
const char kBorealisInstallOverallTimeHistogram2[] =
    "Borealis.Install.OverallTime2";
const char kBorealisStabilityHistogram[] = "Borealis.Stability";
const char kBorealisUninstallNumAttemptsHistogram[] =
    "Borealis.Uninstall.NumAttempts";
const char kBorealisUninstallResultHistogram[] = "Borealis.Uninstall.Result";

void RecordBorealisInstallNumAttemptsHistogram() {
  base::UmaHistogramBoolean(kBorealisInstallNumAttemptsHistogram, true);
}

void RecordBorealisInstallResultHistogram(InstallResult install_result) {
  base::UmaHistogramEnumeration(kBorealisInstallResultHistogram,
                                install_result);
}

void RecordBorealisInstallOverallTimeHistogram(base::TimeDelta install_time) {
  base::UmaHistogramTimes(kBorealisInstallOverallTimeHistogram, install_time);
  base::UmaHistogramLongTimes(kBorealisInstallOverallTimeHistogram2,
                              install_time);
}

void RecordBorealisUninstallNumAttemptsHistogram() {
  base::UmaHistogramBoolean(kBorealisUninstallNumAttemptsHistogram, true);
}

void RecordBorealisUninstallResultHistogram(
    BorealisUninstallResult uninstall_result) {
  base::UmaHistogramEnumeration(kBorealisUninstallResultHistogram,
                                uninstall_result);
}

}  // namespace borealis
