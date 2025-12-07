// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/trace_utils.h"

namespace cc {

perfetto::protos::pbzero::MainFramePipeline* EmitMainFramePipelineStep(
    perfetto::EventContext& ctx,
    BeginMainFrameTraceId commit_id,
    perfetto::protos::pbzero::MainFramePipeline::Step step) {
  perfetto::Flow::ProcessScoped(commit_id.value())(ctx);

  auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
  auto* pipeline = event->set_main_frame_pipeline();

  pipeline->set_step(step);
  pipeline->set_main_frame_id(commit_id.value());
  return pipeline;
}

}  // namespace cc
