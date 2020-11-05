// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_REPORT_THROTTLER_TEST_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_REPORT_THROTTLER_TEST_H_

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_throttler.h"

namespace enterprise_reporting {

class ScopedExtensionRequestReportThrottler {
 public:
  ScopedExtensionRequestReportThrottler();
  ScopedExtensionRequestReportThrottler(base::TimeDelta throttle_time,
                                        base::RepeatingClosure report_trigger);
  ~ScopedExtensionRequestReportThrottler();

  ScopedExtensionRequestReportThrottler(
      const ScopedExtensionRequestReportThrottler&) = delete;
  ScopedExtensionRequestReportThrottler* operator=(
      const ScopedExtensionRequestReportThrottler&) = delete;

  ExtensionRequestReportThrottler* Get();
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_REPORT_THROTTLER_TEST_H_
