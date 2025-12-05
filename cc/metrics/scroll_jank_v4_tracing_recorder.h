// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_TRACING_RECORDER_H_
#define CC_METRICS_SCROLL_JANK_V4_TRACING_RECORDER_H_

#include "cc/cc_export.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "cc/metrics/scroll_jank_v4_result.h"

namespace cc {

// Class responsible for recording trace events for `ScrollJankV4Result`s.
class CC_EXPORT ScrollJankV4TracingRecorder {
 public:
  static void RecordTraceEvents(
      const ScrollJankV4FrameStage::ScrollUpdates& updates,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args,
      const ScrollJankV4Result& result);
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_TRACING_RECORDER_H_
