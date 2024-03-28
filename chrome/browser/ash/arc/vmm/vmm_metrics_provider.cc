// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/vmm_metrics_provider.h"

#include "ash/components/arc/arc_util.h"
#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"

namespace arc {

VmmMetricsProvider::VmmMetricsProvider() {
  auto* client = ash::ConciergeClient::Get();
  if (client) {
    concierge_observation_.Observe(client);
  } else {
    CHECK_IS_TEST();
  }
}
VmmMetricsProvider::~VmmMetricsProvider() = default;

void VmmMetricsProvider::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {
  if (signal.name() != kArcVmName ||
      (signal.status() != vm_tools::concierge::VmStatus::VM_STATUS_RUNNING &&
       signal.status() != vm_tools::concierge::VmStatus::VM_STATUS_STARTING)) {
    return;
  }
  is_arcvm_running_ = true;
}
void VmmMetricsProvider::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  if (signal.name() != kArcVmName) {
    return;
  }
  is_arcvm_running_ = false;
}

void VmmMetricsProvider::OnVmSwapping(
    const vm_tools::concierge::VmSwappingSignal& signal) {
  if (signal.name() != kArcVmName) {
    return;
  }
  CHECK(is_arcvm_running_);
  if (signal.state() == vm_tools::concierge::SWAPPING_OUT) {
    is_arcvm_swapped_out_ = true;
  } else if (signal.state() == vm_tools::concierge::SWAPPING_IN) {
    is_arcvm_swapped_out_ = false;
  }
}

void VmmMetricsProvider::ProvidePreviousSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // TODO(b/315721196): Persist vmm swap stats and upload here.
}

void VmmMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  if (!is_arcvm_running_) {
    return;
  }
  base::UmaHistogramBoolean("Arc.VmmSwappedOut", is_arcvm_swapped_out_);
}

}  // namespace arc
