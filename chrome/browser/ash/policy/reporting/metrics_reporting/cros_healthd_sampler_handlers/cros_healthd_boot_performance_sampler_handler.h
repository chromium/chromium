// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_SAMPLER_HANDLERS_CROS_HEALTHD_BOOT_PERFORMANCE_SAMPLER_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_SAMPLER_HANDLERS_CROS_HEALTHD_BOOT_PERFORMANCE_SAMPLER_HANDLER_H_

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_sampler_handler.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

// Class that handles the resulting data after probing the croshealthd for the
// BootPerformance category.
class CrosHealthdBootPerformanceSamplerHandler
    : public CrosHealthdSamplerHandler {
 public:
  CrosHealthdBootPerformanceSamplerHandler() = default;

  CrosHealthdBootPerformanceSamplerHandler(
      const CrosHealthdBootPerformanceSamplerHandler&) = delete;
  CrosHealthdBootPerformanceSamplerHandler& operator=(
      const CrosHealthdBootPerformanceSamplerHandler&) = delete;

  ~CrosHealthdBootPerformanceSamplerHandler() override;

  // HandleResult converts |result| to MetricData.
  void HandleResult(OptionalMetricCallback callback,
                    cros_healthd::TelemetryInfoPtr result) const override;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_SAMPLER_HANDLERS_CROS_HEALTHD_BOOT_PERFORMANCE_SAMPLER_HANDLER_H_
