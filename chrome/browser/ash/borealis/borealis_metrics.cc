// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"

namespace borealis {

const char kBorealisDiskClientGetDiskInfoResultHistogram[] =
    "Borealis.Disk.Client.GetDiskInfoResult";
const char kBorealisDiskClientRequestSpaceResultHistogram[] =
    "Borealis.Disk.Client.RequestSpaceResult";
const char kBorealisDiskClientReleaseSpaceResultHistogram[] =
    "Borealis.Disk.Client.ReleaseSpaceResult";
extern const char kBorealisDiskClientSpaceRequestedHistogram[] =
    "Borealis.Disk.Client.SpaceRequested";
extern const char kBorealisDiskClientSpaceReleasedHistogram[] =
    "Borealis.Disk.Client.SpaceReleased";
const char kBorealisDiskClientAvailableSpaceAtRequestHistogram[] =
    "Borealis.Disk.Client.AvailableSpaceAtRequest";
const char kBorealisDiskClientNumRequestsPerSessionHistogram[] =
    "Borealis.Disk.Client.NumRequestsPerSesssion";
const char kBorealisDiskStartupAvailableSpaceHistogram[] =
    "Borealis.Disk.Startup.AvailableSpace";
const char kBorealisDiskStartupExpandableSpaceHistogram[] =
    "Borealis.Disk.Startup.ExpandableSpace";
const char kBorealisDiskStartupTotalSpaceHistogram[] =
    "Borealis.Disk.Startup.TotalSpace";
const char kBorealisDiskStartupResultHistogram[] =
    "Borealis.Disk.Startup.Result";
const char kBorealisInstallNumAttemptsHistogram[] =
    "Borealis.Install.NumAttempts";
const char kBorealisInstallResultHistogram[] = "Borealis.Install.Result";
const char kBorealisInstallOverallTimeHistogram[] =
    "Borealis.Install.OverallTime";
const char kBorealisInstallRetriesHistogram[] = "Borealis.Install.Retries";
const char kBorealisShutdownNumAttemptsHistogram[] =
    "Borealis.Shutdown.NumAttempts";
const char kBorealisShutdownResultHistogram[] = "Borealis.Shutdown.Result";
const char kBorealisStabilityHistogram[] = "Borealis.Stability";
const char kBorealisStartupNumAttemptsHistogram[] =
    "Borealis.Startup.NumAttempts";
const char kBorealisStartupResultHistogram[] = "Borealis.Startup.Result";
const char kBorealisStartupOverallTimeHistogram[] =
    "Borealis.Startup.OverallTime";
const char kBorealisUninstallNumAttemptsHistogram[] =
    "Borealis.Uninstall.NumAttempts";
const char kBorealisUninstallResultHistogram[] = "Borealis.Uninstall.Result";

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

void RecordBorealisInstallRetries(int retry_count) {
  base::UmaHistogramCustomCounts(
      kBorealisInstallRetriesHistogram, retry_count,
      /*min=*/0,
      /*exclusive_max=*/BorealisInstaller::kMaxDlcRetries + 1,
      /*buckets=*/BorealisInstaller::kMaxDlcRetries + 1);
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
}

void RecordBorealisShutdownNumAttemptsHistogram() {
  base::UmaHistogramBoolean(kBorealisShutdownNumAttemptsHistogram, true);
}

void RecordBorealisShutdownResultHistogram(
    BorealisShutdownResult shutdown_result) {
  base::UmaHistogramEnumeration(kBorealisShutdownResultHistogram,
                                shutdown_result);
}

void RecordBorealisDiskClientGetDiskInfoResultHistogram(
    BorealisGetDiskInfoResult get_disk_info_result) {
  base::UmaHistogramEnumeration(kBorealisDiskClientGetDiskInfoResultHistogram,
                                get_disk_info_result);
}

void RecordBorealisDiskClientRequestSpaceResultHistogram(
    BorealisResizeDiskResult resize_disk_result) {
  base::UmaHistogramEnumeration(kBorealisDiskClientRequestSpaceResultHistogram,
                                resize_disk_result);
}

void RecordBorealisDiskClientReleaseSpaceResultHistogram(
    BorealisResizeDiskResult resize_disk_result) {
  base::UmaHistogramEnumeration(kBorealisDiskClientReleaseSpaceResultHistogram,
                                resize_disk_result);
}

void RecordBorealisDiskClientSpaceRequestedHistogram(uint64_t bytes_requested) {
  uint64_t megabytes_requested = bytes_requested / (1024 * 1024);
  base::UmaHistogramCustomCounts(kBorealisDiskClientSpaceRequestedHistogram,
                                 megabytes_requested, /*min=*/0, /*max=*/128000,
                                 /*buckets=*/100);
}

void RecordBorealisDiskClientSpaceReleasedHistogram(uint64_t bytes_released) {
  uint64_t megabytes_released = bytes_released / (1024 * 1024);
  base::UmaHistogramCustomCounts(kBorealisDiskClientSpaceReleasedHistogram,
                                 megabytes_released, /*min=*/0, /*max=*/128000,
                                 /*buckets=*/100);
}

void RecordBorealisDiskClientAvailableSpaceAtRequestHistogram(
    uint64_t available_bytes) {
  uint64_t available_megabytes = available_bytes / (1024 * 1024);
  base::UmaHistogramCustomCounts(
      kBorealisDiskClientAvailableSpaceAtRequestHistogram, available_megabytes,
      /*min=*/0, /*max=*/16000, /*buckets=*/100);
}

void RecordBorealisDiskClientNumRequestsPerSessionHistogram(int num_requests) {
  base::UmaHistogramCounts100(kBorealisDiskClientNumRequestsPerSessionHistogram,
                              num_requests);
}

void RecordBorealisDiskStartupAvailableSpaceHistogram(
    uint64_t available_bytes) {
  uint64_t available_megabytes = available_bytes / (1024 * 1024);
  base::UmaHistogramCustomCounts(kBorealisDiskStartupAvailableSpaceHistogram,
                                 available_megabytes,
                                 /*min=*/0, /*max=*/16000, /*buckets=*/100);
}

void RecordBorealisDiskStartupExpandableSpaceHistogram(
    uint64_t expandable_bytes) {
  uint64_t expandable_megabytes = expandable_bytes / (1024 * 1024);
  base::UmaHistogramCustomCounts(kBorealisDiskStartupExpandableSpaceHistogram,
                                 expandable_megabytes, /*min=*/0,
                                 /*max=*/512000,
                                 /*buckets=*/100);
}

void RecordBorealisDiskStartupTotalSpaceHistogram(uint64_t total_bytes) {
  uint64_t total_megabytes = total_bytes / (1024 * 1024);
  base::UmaHistogramCustomCounts(kBorealisDiskStartupTotalSpaceHistogram,
                                 total_megabytes, /*min=*/0,
                                 /*max=*/512000,
                                 /*buckets=*/100);
}

void RecordBorealisDiskStartupResultHistogram(
    BorealisSyncDiskSizeResult disk_result) {
  base::UmaHistogramEnumeration(kBorealisDiskStartupResultHistogram,
                                disk_result);
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
      return stream << "Mount failed";
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
  }
}
