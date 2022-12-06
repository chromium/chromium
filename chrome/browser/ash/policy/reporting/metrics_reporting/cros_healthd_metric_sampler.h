// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_METRIC_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_METRIC_SAMPLER_H_

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_sampler_handler.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

// CrosHealthdMetricSampler samples data from the cros health daemon given a
// specific probe category and metric type.
class CrosHealthdMetricSampler : public Sampler {
 public:
  CrosHealthdMetricSampler(
      std::unique_ptr<CrosHealthdSamplerHandler> handler,
      ash::cros_healthd::mojom::ProbeCategoryEnum probe_category);

  CrosHealthdMetricSampler(const CrosHealthdMetricSampler&) = delete;
  CrosHealthdMetricSampler& operator=(const CrosHealthdMetricSampler&) = delete;

  ~CrosHealthdMetricSampler() override;

  // |MaybeCollect| is called to invoke the healthd probing process.
  void MaybeCollect(OptionalMetricCallback callback) override;

 private:
  // OnHealthInfoReceived calls the handling function to transform the result
  // into MetricData.
  void OnHealthdInfoReceived(OptionalMetricCallback callback,
                             cros_healthd::TelemetryInfoPtr result);

  // CrosHealthdSamplerHandler is an interface that can be used to process the
  // returned result after probing the croshealthd for a particular category.
  std::unique_ptr<CrosHealthdSamplerHandler> handler_;

  // probe_category is the category to probe from the health daemon.
  const ash::cros_healthd::mojom::ProbeCategoryEnum probe_category_;

  base::WeakPtrFactory<CrosHealthdMetricSampler> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_METRIC_SAMPLER_H_
