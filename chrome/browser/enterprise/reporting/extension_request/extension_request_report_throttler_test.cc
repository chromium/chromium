// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_throttler_test.h"

#include "base/callback_helpers.h"
#include "base/time/time.h"

namespace enterprise_reporting {

ScopedExtensionRequestReportThrottler::ScopedExtensionRequestReportThrottler()
    : ScopedExtensionRequestReportThrottler(base::TimeDelta::FromSeconds(10),
                                            base::DoNothing()) {}

ScopedExtensionRequestReportThrottler::ScopedExtensionRequestReportThrottler(
    base::TimeDelta throttle_time,
    base::RepeatingClosure report_trigger) {
  Get()->Enable(throttle_time, report_trigger);
}

ScopedExtensionRequestReportThrottler::
    ~ScopedExtensionRequestReportThrottler() {
  Get()->ResetProfiles();
  Get()->Disable();
}

ExtensionRequestReportThrottler* ScopedExtensionRequestReportThrottler::Get() {
  return ExtensionRequestReportThrottler::Get();
}

}  // namespace enterprise_reporting
