// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TRACE_UTILS_H_
#define CC_TREES_TRACE_UTILS_H_

#include "base/trace_event/typed_macros.h"
#include "cc/trees/begin_main_frame_trace_id.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

// Emits trace event arguments for slices corresponding to the steps of the
// pipeline generating main frames.
perfetto::protos::pbzero::MainFramePipeline* EmitMainFramePipelineStep(
    perfetto::EventContext& ctx,
    BeginMainFrameTraceId commit_id,
    perfetto::protos::pbzero::MainFramePipeline::Step step);

}  // namespace cc

#endif  // CC_TREES_TRACE_UTILS_H_
