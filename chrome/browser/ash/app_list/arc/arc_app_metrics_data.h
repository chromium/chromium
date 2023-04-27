// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_METRICS_DATA_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_METRICS_DATA_H_

#include <map>
#include <string>

#include "base/time/time.h"

namespace arc {

class ArcAppMetricsData {
 public:
  explicit ArcAppMetricsData(const std::string& histogram_base);
  ~ArcAppMetricsData();

  // Records the install start time for a specific app.
  void recordAppInstallStartTime(const std::string& app_name);

  // Reports install time delta for an app to UMA.
  void maybeReportInstallTimeDelta(const std::string& app_name);

  // Reports the number of incomplete app installs to UMA.
  void reportMetrics();

 private:
  std::map<std::string, base::TimeTicks> install_start_time_map_;
  int32_t num_requests_ = 0;
  const std::string histogram_base_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_METRICS_DATA_H_
