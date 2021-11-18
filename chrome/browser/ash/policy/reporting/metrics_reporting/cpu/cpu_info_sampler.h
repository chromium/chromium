// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CPU_CPU_INFO_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CPU_CPU_INFO_SAMPLER_H_

#include "components/reporting/metrics/sampler.h"

#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace cros_healthd = chromeos::cros_healthd::mojom;

namespace reporting {

using CpuInfoFetcher = base::RepeatingCallback<void(
    base::OnceCallback<void(cros_healthd::TelemetryInfoPtr)>)>;

// CpuInfoSampler collects info related to a devices CPUs which aren't
// expected to change. This information will only be collected and reported
// once, on system startup or on a policy setting change from false to true.
class CpuInfoSampler : public Sampler {
 public:
  // For use in production. This constructor follows the default means of
  // metric sampling.
  CpuInfoSampler();

  // For use in testing. This function allows for a custom cpu info collection
  // method to be passed in.
  explicit CpuInfoSampler(CpuInfoFetcher);

  CpuInfoSampler(const CpuInfoSampler&) = delete;
  CpuInfoSampler& operator=(const CpuInfoSampler&) = delete;

  ~CpuInfoSampler() override;

  // This is called to invoke the cpu info fetching process.
  void Collect(MetricCallback callback) override;

 private:
  // This internal function runs the cpu info fetcher received from the
  // constructor.
  static void FetchCpuInfo(
      base::OnceCallback<void(cros_healthd::TelemetryInfoPtr)>
          healthd_callback);

  CpuInfoFetcher cpu_info_fetcher_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CPU_CPU_INFO_SAMPLER_H_
