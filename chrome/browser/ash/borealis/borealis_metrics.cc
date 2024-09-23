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
const char kBorealisInstallSourceHistogram[] = "Borealis.Install.Source";
const char kBorealisInstallOverallTimeHistogram[] =
    "Borealis.Install.OverallTime";
// Same as Borealis.Install.OverallTime, but with more appropriate bucket sizes.
const char kBorealisInstallOverallTimeHistogram2[] =
    "Borealis.Install.OverallTime2";
const char kBorealisLaunchSourceHistogram[] = "Borealis.Launch.Source";
const char kBorealisShutdownNumAttemptsHistogram[] =
    "Borealis.Shutdown.NumAttempts";
const char kBorealisShutdownResultHistogram[] = "Borealis.Shutdown.Result";
const char kBorealisStabilityHistogram[] = "Borealis.Stability";
const char kBorealisStartupNumAttemptsHistogram[] =
    "Borealis.Startup.NumAttempts";
const char kBorealisStartupResultHistogram[] = "Borealis.Startup.Result";
const char kBorealisStartupOverallTimeHistogram[] =
    "Borealis.Startup.OverallTime";
// Same as Borealis.Startup.OverallTime, but with more appropriate bucket sizes.
const char kBorealisStartupOverallTimeHistogram2[] =
    "Borealis.Startup.OverallTime2";
const char kBorealisStartupTimeToFirstWindowHistogram[] =
    "Borealis.Startup.TimeToFirstWindow";
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

void RecordBorealisInstallSourceHistogram(BorealisLaunchSource install_source) {
  base::UmaHistogramEnumeration(kBorealisInstallSourceHistogram,
                                install_source);
}

void RecordBorealisInstallOverallTimeHistogram(base::TimeDelta install_time) {
  base::UmaHistogramTimes(kBorealisInstallOverallTimeHistogram, install_time);
  base::UmaHistogramLongTimes(kBorealisInstallOverallTimeHistogram2,
                              install_time);
}

void RecordBorealisLaunchSourceHistogram(BorealisLaunchSource launch_source) {
  base::UmaHistogramEnumeration(kBorealisLaunchSourceHistogram, launch_source);
}

void RecordBorealisUninstallNumAttemptsHistogram() {
  base::UmaHistogramBoolean(kBorealisUninstallNumAttemptsHistogram, true);
}

void RecordBorealisUninstallResultHistogram(
    BorealisUninstallResult uninstall_result) {
  base::UmaHistogramEnumeration(kBorealisUninstallResultHistogram,
                                uninstall_result);
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
  base::UmaHistogramMediumTimes(kBorealisStartupOverallTimeHistogram2,
                                startup_time);
}

void RecordBorealisStartupTimeToFirstWindowHistogram(
    base::TimeDelta startup_time) {
  base::UmaHistogramMediumTimes(kBorealisStartupTimeToFirstWindowHistogram,
                                startup_time);
}

void RecordBorealisShutdownNumAttemptsHistogram() {
  base::UmaHistogramBoolean(kBorealisShutdownNumAttemptsHistogram, true);
}

void RecordBorealisShutdownResultHistogram(
    BorealisShutdownResult shutdown_result) {
  base::UmaHistogramEnumeration(kBorealisShutdownResultHistogram,
                                shutdown_result);
}

}  // namespace borealis

std::ostream& operator<<(std::ostream& stream,
                         borealis::BorealisStartupResult result) {
  switch (result) {
    case borealis::BorealisStartupResult::kSuccess:
      return stream << "Success";
    case borealis::BorealisStartupResult::kCancelled:
      return stream << "Cancelled";
    case borealis::BorealisStartupResult::kDiskImageFailed:
      return stream << "Disk Image failed";
    case borealis::BorealisStartupResult::kStartVmFailed:
      return stream << "Start VM failed";
    case borealis::BorealisStartupResult::kAwaitBorealisStartupFailed:
      return stream << "Await Borealis Startup failed";
    case borealis::BorealisStartupResult::kSyncDiskFailed:
      return stream << "Syncing Disk failed";
    case borealis::BorealisStartupResult::kRequestWaylandFailed:
      return stream << "Request Wayland failed";
    case borealis::BorealisStartupResult::kDisallowed:
      return stream << "Borealis is not allowed";
    case borealis::BorealisStartupResult::kDlcCancelled:
      return stream << "DLC install was cancelled";
    case borealis::BorealisStartupResult::kDlcOffline:
      return stream << "Device is offline";
    case borealis::BorealisStartupResult::kDlcNeedUpdateError:
      return stream
             << "DLC service couldn't find an image at the correct version";
    case borealis::BorealisStartupResult::kDlcNeedRebootError:
      return stream << "Device needs to be rebooted";
    case borealis::BorealisStartupResult::kDlcNeedSpaceError:
      return stream << "Device needs more space to install DLC";
    case borealis::BorealisStartupResult::kDlcBusyError:
      return stream << "DLC service is busy";
    case borealis::BorealisStartupResult::kDlcInternalError:
      return stream << "DLC reported an internal error";
    case borealis::BorealisStartupResult::kDlcUnsupportedError:
      return stream << "Borealis DLC is not supported";
    case borealis::BorealisStartupResult::kDlcUnknownError:
      return stream << "DLC service ran into an unknown error";
    case borealis::BorealisStartupResult::kConciergeUnavailable:
      return stream << "Concierge is unavailable";
    case borealis::BorealisStartupResult::kEmptyDiskResponse:
      return stream << "Concierge returned an empty disk response";
    case borealis::BorealisStartupResult::kStartVmEmptyResponse:
      return stream << "Concierge returned an empty startup request";
  }
}
