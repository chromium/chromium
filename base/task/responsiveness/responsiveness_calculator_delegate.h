// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_RESPONSIVENESS_RESPONSIVENESS_CALCULATOR_DELEGATE_H_
#define BASE_TASK_RESPONSIVENESS_RESPONSIVENESS_CALCULATOR_DELEGATE_H_

#include <stddef.h>

#include "base/base_export.h"

namespace base {

// Allows the embedder to be notified when the responsiveness metric
// (Browser.MainThreadsCongestion) is emitted.
class BASE_EXPORT ResponsivenessCalculatorDelegate {
 public:
  // Invoked whenever a measurement interval ended, which includes when the
  // application is suspended and the current interval should be discarded.
  virtual void OnMeasurementIntervalEnded() = 0;
  // Invoked when a sample is emitted to the responsiveness histogram.
  virtual void OnResponsivenessEmitted(int num_congested_slices,
                                       int min,
                                       int exclusive_max,
                                       size_t buckets) = 0;

  virtual ~ResponsivenessCalculatorDelegate() = default;
};

}  // namespace base

#endif  // BASE_TASK_RESPONSIVENESS_RESPONSIVENESS_CALCULATOR_DELEGATE_H_
