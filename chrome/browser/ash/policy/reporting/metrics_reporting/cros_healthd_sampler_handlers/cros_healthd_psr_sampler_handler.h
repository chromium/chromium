// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_SAMPLER_HANDLERS_CROS_HEALTHD_PSR_SAMPLER_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_SAMPLER_HANDLERS_CROS_HEALTHD_PSR_SAMPLER_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_sampler_handler.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

namespace cros_healthd = ::ash::cros_healthd::mojom;

// Class that handles the resulting data after probing the croshealthd for the
// PSR category. We use this handler to explore runtime counters provided by
// PSR.
class CrosHealthdPsrSamplerHandler : public CrosHealthdSamplerHandler {
 public:
  CrosHealthdPsrSamplerHandler();

  CrosHealthdPsrSamplerHandler(const CrosHealthdPsrSamplerHandler&) = delete;
  CrosHealthdPsrSamplerHandler& operator=(const CrosHealthdPsrSamplerHandler&) =
      delete;

  ~CrosHealthdPsrSamplerHandler() override;

  // CrosHealthdSamplerHandler override
  void HandleResult(OptionalMetricCallback callback,
                    cros_healthd::TelemetryInfoPtr result) const override;

 protected:
  // Request PSR info from healthd again to retry for at most num_retries_left
  // times. Should only be overridden in tests.
  virtual void Retry(OptionalMetricCallback callback,
                     size_t num_retries_left) const;

 private:
  // Implementation of `HandleResult`. If it fails to obtain PSR info, it will
  // get PSR info from healthd again for at most `num_retries_left` times with
  // itself as the callback. A zero `num_retries_left` implies no retry on
  // failure.
  void HandleResultImpl(OptionalMetricCallback callback,
                        size_t num_retries_left,
                        cros_healthd::TelemetryInfoPtr result) const;

  SEQUENCE_CHECKER(sequence_checker_);

 protected:
  base::TimeDelta wait_time_ GUARDED_BY_CONTEXT(sequence_checker_){
      base::Seconds(10)};

 private:
  base::WeakPtrFactory<CrosHealthdPsrSamplerHandler> weak_ptr_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_SAMPLER_HANDLERS_CROS_HEALTHD_PSR_SAMPLER_HANDLER_H_
