// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_FUCHSIA_H_
#define BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_FUCHSIA_H_

#include <fuchsia/memorypressure/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "base/util/memory_pressure/system_memory_pressure_evaluator.h"

namespace util {
class MemoryPressureVoter;

// Registers with the fuchsia.memorypressure.Provider to be notified of changes
// to the system memory pressure level. Votes are sent immediately when
// memory pressure becomes MODERATE or CRITICAL, and periodically until
// memory pressure drops back down to NONE. No notifications are sent at NONE
// level.
class SystemMemoryPressureEvaluatorFuchsia
    : public SystemMemoryPressureEvaluator,
      public fuchsia::memorypressure::Watcher {
 public:
  using SystemMemoryPressureEvaluator::SendCurrentVote;

  // The period at which the system is re-notified when the pressure is not
  // none.
  static const base::TimeDelta kRenotifyVotePeriod;

  explicit SystemMemoryPressureEvaluatorFuchsia(
      std::unique_ptr<util::MemoryPressureVoter> voter);

  ~SystemMemoryPressureEvaluatorFuchsia() override;

  SystemMemoryPressureEvaluatorFuchsia(
      const SystemMemoryPressureEvaluatorFuchsia&) = delete;
  SystemMemoryPressureEvaluatorFuchsia& operator=(
      const SystemMemoryPressureEvaluatorFuchsia&) = delete;

 private:
  // fuchsia::memorypressure::Watcher implementation.
  void OnLevelChanged(fuchsia::memorypressure::Level level,
                      OnLevelChangedCallback callback) override;

  fidl::Binding<fuchsia::memorypressure::Watcher> binding_;

  // Timer that will re-notify with the current vote at regular interval.
  base::RepeatingTimer renotify_current_vote_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace util

#endif  // BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_FUCHSIA_H_
