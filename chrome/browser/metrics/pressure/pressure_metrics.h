// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PRESSURE_PRESSURE_METRICS_H_
#define CHROME_BROWSER_METRICS_PRESSURE_PRESSURE_METRICS_H_

#include <optional>

#include "base/files/file_path.h"

class PressureMetrics {
 public:
  PressureMetrics(const char* histogram_name, base::FilePath metric_path);
  ~PressureMetrics();

  // Percentages of pressure. A value of 0.12 is 0.12%.
  struct Sample {
    double some_avg10;
    double some_avg60;
    double some_avg300;
    double full_avg10;
    double full_avg60;
    double full_avg300;
  };
  std::optional<Sample> CollectCurrentPressure() const;

  void EmitCounters(const Sample& sample) const;
  void ReportToUMA(const Sample& sample) const;

 private:
  const char* histogram_name_;
  base::FilePath metric_path_;
};

#endif  // CHROME_BROWSER_METRICS_PRESSURE_PRESSURE_METRICS_H_
