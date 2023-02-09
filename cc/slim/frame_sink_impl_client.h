// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_FRAME_SINK_IMPL_CLIENT_H_
#define CC_SLIM_FRAME_SINK_IMPL_CLIENT_H_

#include "base/containers/flat_set.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/resource_id.h"

namespace cc::slim {

class FrameSinkImplClient {
 public:
  virtual ~FrameSinkImplClient() = default;

  // Notification for the client to generate a CompositorFrame.
  // Client can either return false in which case the output params are ignored.
  // Otherwise client needs to populate the output parameters.
  // Client should update `SetNeedsBeginFrame` if the state changes as the
  // result of this call.
  virtual bool BeginFrame(const viz::BeginFrameArgs& args,
                          viz::CompositorFrame& out_frame,
                          base::flat_set<viz::ResourceId>& out_resource_ids,
                          viz::HitTestRegionList& out_hit_test_region_list) = 0;
  // Notification that the previous CompositorFrame given to
  // SubmitCompositorFrame() has been processed and that another frame
  // can be submitted.
  virtual void DidReceiveCompositorFrameAck() = 0;
  // Called when a frame is submitted. This should normally match with
  // `DidReceiveCompositorFrameAck` except when sink is lost.
  virtual void DidSubmitCompositorFrame() = 0;
  // Provide information on a presented frame.
  virtual void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) = 0;
  // FrameSink is lost. Some normally expected callbacks such as
  // `DidReceiveCompositorFrameAck` will not happen after this.
  virtual void DidLoseLayerTreeFrameSink() = 0;
};

}  // namespace cc::slim

#endif  // CC_SLIM_FRAME_SINK_IMPL_CLIENT_H_
