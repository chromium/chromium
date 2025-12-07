// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace safe_browsing::client_side_detection {

void LogOnDeviceModelExecutionSuccessAndTime(
    bool success,
    base::TimeTicks session_execution_start_time) {
  base::UmaHistogramBoolean("SBClientPhishing.OnDeviceModelExecutionSuccess",
                            success);
  base::UmaHistogramMediumTimes(
      "SBClientPhishing.OnDeviceModelExecutionDuration",
      base::TimeTicks::Now() - session_execution_start_time);
}

void LogOnDeviceModelSessionCreationTime(
    base::TimeTicks session_creation_start_time) {
  base::UmaHistogramMediumTimes(
      "SBClientPhishing.OnDeviceModelSessionCreationTime",
      base::TimeTicks::Now() - session_creation_start_time);
}

void LogOnDeviceModelFetchTime(base::TimeTicks on_device_fetch_time) {
  base::UmaHistogramLongTimes("SBClientPhishing.OnDeviceModelFetchTime",
                              base::TimeTicks::Now() - on_device_fetch_time);
}

void LogOnDeviceModelDownloadSuccess(bool success) {
  base::UmaHistogramBoolean("SBClientPhishing.OnDeviceModelDownloadSuccess",
                            success);
}

void LogOnDeviceModelSessionAliveOnDelegateShutdown(bool session_alive) {
  base::UmaHistogramBoolean(
      "SBClientPhishing.OnDeviceModelSessionAliveOnDelegateShutdown",
      session_alive);
}

}  // namespace safe_browsing::client_side_detection
