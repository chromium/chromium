// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_UMA_METRICS_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_UMA_METRICS_UTIL_H_

namespace enterprise_connectors {

extern const char kUniquifierUmaLabel[];

extern const char kDownloadsRoutingDestinationUmaLabel[];

extern const char kBoxDownloadsRoutingStatusUmaLabel[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "Enterprise.FileSystem.DownloadsRouting"
enum class EnterpriseFileSystemDownloadsRoutingDestination {
  NOT_ROUTED = 0,
  ROUTED_TO_BOX = 1,
  // Update kMaxValue when adding other Providers here.
  kMaxValue = ROUTED_TO_BOX,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "Enterprise.FileSystem.DownloadsRouting.Box""
enum class EnterpriseFileSystemDownloadsRoutingStatus {
  ROUTING_SUCCEEDED = 0,
  ROUTING_CANCELED = 1,
  ROUTING_FAILED_BROWSER_ERROR = 2,
  ROUTING_FAILED_FILE_ERROR = 3,
  ROUTING_FAILED_NETWORK_ERROR = 4,
  ROUTING_FAILED_SERVICE_PROVIDER_ERROR = 5,
  ROUTING_FAILED_SERVICE_PROVIDER_OUTAGE = 6,
  kMaxValue = ROUTING_FAILED_SERVICE_PROVIDER_OUTAGE,
};

// Helper function to log DownloadsRouting status to UMA.
void UmaLogDownloadsRoutingStatus(
    EnterpriseFileSystemDownloadsRoutingStatus status);

// Helper function to log DownloadsRouting destination to UMA.
void UmaLogDownloadsRoutingDestination(
    EnterpriseFileSystemDownloadsRoutingDestination destination);

}  // namespace enterprise_connectors
#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_UMA_METRICS_UTIL_H_
