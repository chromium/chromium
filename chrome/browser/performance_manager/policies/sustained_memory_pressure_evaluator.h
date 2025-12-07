// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_SUSTAINED_MEMORY_PRESSURE_EVALUATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_SUSTAINED_MEMORY_PRESSURE_EVALUATOR_H_

#include "base/functional/callback.h"
#include "base/timer/timer.h"

namespace performance_manager::policies {

class SustainedMemoryPressureEvaluator {
 public:
  using OnSustainedMemoryPressureCallback = base::RepeatingCallback<void(bool)>;
  explicit SustainedMemoryPressureEvaluator(
      OnSustainedMemoryPressureCallback on_sustained_memory_pressure_callback);
  ~SustainedMemoryPressureEvaluator();

  void OnCheckMemoryPressure();

  void OnSustainedMemoryPressure();

 private:
  OnSustainedMemoryPressureCallback on_sustained_memory_pressure_callback_;

  base::RepeatingTimer check_pressure_timer_;
  base::RetainingOneShotTimer on_sustained_memory_pressure_timer_;

  bool is_under_memory_pressure_ = false;
  bool is_under_sustained_memory_pressure_ = false;
};

}  //  namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_SUSTAINED_MEMORY_PRESSURE_EVALUATOR_H_
