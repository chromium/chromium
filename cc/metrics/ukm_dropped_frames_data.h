// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_UKM_DROPPED_FRAMES_DATA_H_
#define CC_METRICS_UKM_DROPPED_FRAMES_DATA_H_

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/shared_metrics_buffer.h"

namespace cc {

// PercentDroppedFrames4 UKM metric
// exported from frame_sequence_metrics.cc
struct CC_EXPORT UkmDroppedFramesData {
  UkmDroppedFramesData();

  double percent_dropped_frames = 0.0;
};

using UkmDroppedFramesDataShared = SharedMetricsBuffer<UkmDroppedFramesData>;

}  // namespace cc

#endif  // CC_METRICS_UKM_DROPPED_FRAMES_DATA_H_
