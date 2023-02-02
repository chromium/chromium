// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_FRAME_SINK_IMPL_CLIENT_H_
#define CC_SLIM_FRAME_SINK_IMPL_CLIENT_H_

#include "base/containers/flat_set.h"
#include "cc/resources/ui_resource_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace cc::slim {

class FrameSinkImplClient {
 public:
  virtual ~FrameSinkImplClient() = default;

  virtual bool BeginFrame(const viz::BeginFrameArgs& args,
                          viz::CompositorFrame& out_frame,
                          base::flat_set<cc::UIResourceId>& out_resource_ids,
                          viz::HitTestRegionList& out_hit_test_region_list) = 0;
  virtual void DidReceiveCompositorFrameAck() = 0;
  virtual void DidSubmitCompositorFrame() = 0;
  virtual void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) = 0;
  virtual void DidLoseLayerTreeFrameSink() = 0;
};

}  // namespace cc::slim

#endif  // CC_SLIM_FRAME_SINK_IMPL_CLIENT_H_
