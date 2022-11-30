// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/uma_metrics_util.h"
#include "base/metrics/histogram_functions.h"

namespace enterprise_connectors {

const char kUniquifierUmaLabel[] = "Enterprise.FileSystem.Uniquifier";

const char kDownloadsRoutingDestinationUmaLabel[] =
    "Enterprise.FileSystem.DownloadsRouting";

const char kBoxDownloadsRoutingStatusUmaLabel[] =
    "Enterprise.FileSystem.DownloadsRouting.Box";

void UmaLogDownloadsRoutingStatus(
    EnterpriseFileSystemDownloadsRoutingStatus status) {
  base::UmaHistogramEnumeration(kBoxDownloadsRoutingStatusUmaLabel, status);
}

void UmaLogDownloadsRoutingDestination(
    EnterpriseFileSystemDownloadsRoutingDestination destination) {
  base::UmaHistogramEnumeration(kDownloadsRoutingDestinationUmaLabel,
                                destination);
}
}  // namespace enterprise_connectors
