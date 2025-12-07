// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/sustained_memory_pressure_evaluator.h"

#include "base/byte_count.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"

namespace performance_manager::policies {

// Returns true if the available memory is under 15%. Only implemented on
// Windows.
bool IsMemoryPressure() {
#if BUILDFLAG(IS_WIN)
  base::SystemMemoryInfo info;
  if (!base::GetSystemMemoryInfo(&info)) {
    // Cannot get system memory info, do nothing.
    return false;
  }

  base::ByteCount total = info.total;
  base::ByteCount avail = info.avail_phys;

  // This is unexpected, do nothing.
  if (!total.is_positive()) {
    return false;
  }

  int available_percent =
      static_cast<int>(avail.InBytesF() / total.InBytesF() * 100.0);

  // Consider memory pressure when under the threshold.
  return available_percent <
         features::kSustainedPMUrgentDiscarding_PercentAvailableMemory.Get();

#else  // BUILDFLAG(IS_WIN)
  return false;
#endif
}

SustainedMemoryPressureEvaluator::SustainedMemoryPressureEvaluator(
    OnSustainedMemoryPressureCallback on_sustained_memory_pressure_callback)
    : on_sustained_memory_pressure_callback_(
          std::move(on_sustained_memory_pressure_callback)),
      check_pressure_timer_(
          FROM_HERE,
          features::kSustainedPMUrgentDiscarding_CheckPressureDelay.Get(),
          base::BindRepeating(
              &SustainedMemoryPressureEvaluator::OnCheckMemoryPressure,
              base::Unretained(this))),
      on_sustained_memory_pressure_timer_(
          FROM_HERE,
          features::kSustainedPMUrgentDiscarding_SustainedPressureDelay.Get(),
          base::BindRepeating(
              &SustainedMemoryPressureEvaluator::OnSustainedMemoryPressure,
              base::Unretained(this))) {}

SustainedMemoryPressureEvaluator::~SustainedMemoryPressureEvaluator() = default;

void SustainedMemoryPressureEvaluator::OnCheckMemoryPressure() {
  const bool was_under_memory_pressure_ =
      std::exchange(is_under_memory_pressure_, IsMemoryPressure());

  // Nothing to do if the state did not change.
  if (was_under_memory_pressure_ == is_under_memory_pressure_) {
    return;
  }

  if (is_under_memory_pressure_) {
    // Memory pressure state just started. If it perdures, then the memory
    // pressure state is considered to be "sustained".
    on_sustained_memory_pressure_timer_.Reset();
  } else {
    // No longer under memory pressure. If we were under sustained memory
    // pressure, stop it.
    on_sustained_memory_pressure_timer_.Stop();
    if (is_under_sustained_memory_pressure_) {
      is_under_sustained_memory_pressure_ = false;
      on_sustained_memory_pressure_callback_.Run(false);
    }
  }
}

void SustainedMemoryPressureEvaluator::OnSustainedMemoryPressure() {
  is_under_sustained_memory_pressure_ = true;
  on_sustained_memory_pressure_callback_.Run(true);
}

}  //  namespace performance_manager::policies
