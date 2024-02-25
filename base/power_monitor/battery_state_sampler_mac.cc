// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_state_sampler.h"

#include "base/power_monitor/iopm_power_source_sampling_event_source.h"

namespace base {

// static
std::unique_ptr<SamplingEventSource>
BatteryStateSampler::CreateSamplingEventSource() {
  return std::make_unique<IOPMPowerSourceSamplingEventSource>();
}

}  // namespace base
