// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_METRICS_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_METRICS_UTIL_H_

#include <map>
#include <string>

#include "base/time/time.h"

namespace arc {

class ArcAppMetricsData;

// Helper class to record metrics for app installs.
class ArcAppMetricsUtil {
 public:
  ArcAppMetricsUtil();
  ~ArcAppMetricsUtil();

  // Records the install start time for a specific app.
  void recordAppInstallStartTime(const std::string& app_name,
                                 bool is_controlled_by_policy);

  // Reports install time delta for an app to UMA.
  void maybeReportInstallTimeDelta(const std::string& app_name,
                                   bool is_controlled_by_policy);

  // Reports the number of incomplete app installs to UMA.
  void reportMetrics();

 private:
  std::unique_ptr<ArcAppMetricsData> manual_install_data_;
  std::unique_ptr<ArcAppMetricsData> policy_install_data_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_METRICS_UTIL_H_
