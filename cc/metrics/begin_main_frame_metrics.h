// Copyright 2019 The Chromium Authors. All rights reserved.
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
// TODO(schenney): Include work done in LayerTreeHost::AnimateLayers?
struct CC_EXPORT BeginMainFrameMetrics {
  base::TimeDelta handle_input_events;
  base::TimeDelta animate;
  base::TimeDelta style_update;
  base::TimeDelta layout_update;
  base::TimeDelta prepaint;
  base::TimeDelta composite;
  base::TimeDelta paint;
  base::TimeDelta scrolling_coordinator;
  base::TimeDelta composite_commit;
  base::TimeDelta update_layers;

  BeginMainFrameMetrics();

  BeginMainFrameMetrics(const BeginMainFrameMetrics& other);
};

}  // namespace cc

#endif  // CC_METRICS_BEGIN_MAIN_FRAME_METRICS_H_
