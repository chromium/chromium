// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_BEGIN_MAIN_FRAME_METRICS_H_
#define CC_METRICS_BEGIN_MAIN_FRAME_METRICS_H_

#include "base/time/time.h"
#include "cc/cc_export.h"

namespace cc {

// Latency timing data for Main Frame lifecycle updates triggered by cc.
// The data is captured in LocalFrameViewUKMAggregator and passed back through
// the proxy when a main frame ends. LayerTreeHost updates the update_layers_
// value in LayerTreeHost::UpdateLayers.
struct CC_EXPORT BeginMainFrameMetrics {
  base::TimeDelta handle_input_events;
  base::TimeDelta animate;
  base::TimeDelta style_update;
  base::TimeDelta layout_update;
  base::TimeDelta accessibility;
  base::TimeDelta prepaint;
  base::TimeDelta compositing_inputs;
  base::TimeDelta paint;
  base::TimeDelta composite_commit;
  base::TimeDelta update_layers;
  // True if we should measure smoothness in TotalFrameCounter and
  // DroppedFrameCounter. Currently true when first contentful paint is done.
  bool should_measure_smoothness = false;

  BeginMainFrameMetrics();

  BeginMainFrameMetrics(const BeginMainFrameMetrics& other);
  BeginMainFrameMetrics& operator=(const BeginMainFrameMetrics& other);
};

}  // namespace cc

#endif  // CC_METRICS_BEGIN_MAIN_FRAME_METRICS_H_
