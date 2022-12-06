// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_SAMPLER_HANDLERS_CROS_HEALTHD_SAMPLER_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_SAMPLER_HANDLERS_CROS_HEALTHD_SAMPLER_HANDLER_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

// CrosHealthdSamplerHandler is an interface that can be used to process the
// returned result after probing the croshealthd for a particular category.
class CrosHealthdSamplerHandler {
 public:
  // MetricType enumerates the potential metric types that can be probed from
  // healthd.
  enum class MetricType { kInfo = 0, kTelemetry = 1 };

  virtual ~CrosHealthdSamplerHandler() = default;

  // Converts |result| to MetricData and passes it to |callback|. This method is
  // used when there's only one possible MetricType for the metric category.
  virtual void HandleResult(OptionalMetricCallback callback,
                            cros_healthd::TelemetryInfoPtr result) const = 0;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_SAMPLER_HANDLERS_CROS_HEALTHD_SAMPLER_HANDLER_H_
