// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/real_time_report_controller_android.h"

namespace enterprise_reporting {

RealTimeReportControllerAndroid::RealTimeReportControllerAndroid() = default;
RealTimeReportControllerAndroid::~RealTimeReportControllerAndroid() = default;

void RealTimeReportControllerAndroid::StartWatchingExtensionRequestIfNeeded() {
  // No-op because extensions are not supported on Android.
}

void RealTimeReportControllerAndroid::StopWatchingExtensionRequest() {
  // No-op because extensions are not supported on Android.
}

}  // namespace enterprise_reporting
