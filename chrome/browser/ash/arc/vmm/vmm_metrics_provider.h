// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_VMM_VMM_METRICS_PROVIDER_H_
#define CHROME_BROWSER_ASH_ARC_VMM_VMM_METRICS_PROVIDER_H_

#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"
#include "components/metrics/metrics_provider.h"

namespace arc {

class VmmMetricsProvider : public ash::ConciergeClient::VmObserver,
                           public metrics::MetricsProvider {
 public:
  VmmMetricsProvider();

  VmmMetricsProvider(const VmmMetricsProvider&) = delete;
  VmmMetricsProvider& operator=(const VmmMetricsProvider&) = delete;

  ~VmmMetricsProvider() override;

  // ash::ConciergeClient::VmObserver overrides:
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;
  void OnVmSwapping(
      const vm_tools::concierge::VmSwappingSignal& signal) override;

  // metrics::MetricsProvider overrides:
  void ProvidePreviousSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  // The ARCVM swap status reported by concierge.
  bool is_arcvm_running_ = false;
  bool is_arcvm_swapped_out_ = false;
  base::ScopedObservation<ash::ConciergeClient,
                          ash::ConciergeClient::VmObserver>
      concierge_observation_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_VMM_VMM_METRICS_PROVIDER_H_
