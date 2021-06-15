// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_UTIL_CRITICAL_POLICY_SECTION_METRICS_WIN_H_
#define CHROME_BROWSER_ENTERPRISE_UTIL_CRITICAL_POLICY_SECTION_METRICS_WIN_H_

namespace chrome {
namespace enterprise_util {

// Runs a task on a background sequence to measure how long it takes to
// acquire the lock to access policies in the Windows registry.
void MeasureAndReportCriticalPolicySectionAcquisition();

}  // namespace enterprise_util
}  // namespace chrome

#endif  // CHROME_BROWSER_ENTERPRISE_UTIL_CRITICAL_POLICY_SECTION_METRICS_WIN_H_
