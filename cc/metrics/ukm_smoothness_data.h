// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_UKM_SMOOTHNESS_DATA_H_
#define CC_METRICS_UKM_SMOOTHNESS_DATA_H_

#include "device/base/synchronization/one_writer_seqlock.h"

namespace cc {

// The smoothness metrics, containing the score measured using various
// normalization strategies. The normalization strategies are detailed in
// https://docs.google.com/document/d/1ENJXn2bPqvxycnVS9X35qDu1642DQyz42upj5ETOhSs/preview
struct UkmSmoothnessData {
  double avg_smoothness = 0.0;
  double worst_smoothness = 0.0;
  double above_threshold = 0.0;
  double percentile_95 = 0.0;
};

// The struct written in shared memory to transport UkmSmoothnessData across
// processes. |data| is protected by the sequence-lock |seq_lock|.
struct UkmSmoothnessDataShared {
  device::OneWriterSeqLock seq_lock;
  struct UkmSmoothnessData data;
};

}  // namespace cc

#endif  // CC_METRICS_UKM_SMOOTHNESS_DATA_H_
