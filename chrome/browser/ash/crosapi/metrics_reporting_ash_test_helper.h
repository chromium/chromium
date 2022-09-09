// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_METRICS_REPORTING_ASH_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_CROSAPI_METRICS_REPORTING_ASH_TEST_HELPER_H_

#include <memory>

namespace crosapi {

class MetricsReportingAsh;

std::unique_ptr<MetricsReportingAsh> CreateTestMetricsReportingAsh();

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_METRICS_REPORTING_ASH_TEST_HELPER_H_
