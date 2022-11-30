// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_METRIC_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_METRIC_SAMPLER_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

// CrosHealthdMetricSampler samples data from the cros health daemon given a
// specific probe category and metric type.
class CrosHealthdMetricSampler : public Sampler {
 public:
  // MetricType enumerates the potential metric types that can be probed from
  // healthd.
  enum class MetricType { kInfo = 0, kTelemetry = 1 };

  explicit CrosHealthdMetricSampler(
      ash::cros_healthd::mojom::ProbeCategoryEnum probe_category,
      MetricType metric_type);

  CrosHealthdMetricSampler(const CrosHealthdMetricSampler&) = delete;
  CrosHealthdMetricSampler& operator=(const CrosHealthdMetricSampler&) = delete;

  ~CrosHealthdMetricSampler() override;

  // |MaybeCollect| is called to invoke the healthd probing process.
  void MaybeCollect(OptionalMetricCallback callback) override;

 private:
  // probe_category is the category to probe from the health daemon.
  const ash::cros_healthd::mojom::ProbeCategoryEnum probe_category_;

  // metric_type is the type of data to gather. This is necessary since some
  // probe categories have both info and telemetry in their result.
  const MetricType metric_type_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_METRIC_SAMPLER_H_
