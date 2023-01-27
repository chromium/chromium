// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_LAYER_TREE_CLIENT_H_
#define CC_SLIM_LAYER_TREE_CLIENT_H_

#include "base/component_export.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc::slim {

// Implemented by client to respond to requests by LayerTree.
class COMPONENT_EXPORT(CC_SLIM) LayerTreeClient {
 public:
  virtual ~LayerTreeClient() = default;

  // A new frame is about to be produced. Client can use the timestamp in
  // `vz::BeginFrameArgs` to perform animation updates to layers. This
  // generally happens as a result of modifying the layer tree. Client can
  // also request one off frames with `LayerTree::SetNeedsAnimate` or
  // `LayerTree::SetNeedsRedraw`.
  virtual void BeginFrame(const viz::BeginFrameArgs& args) = 0;

  // A new is submitted to GPU/viz. Note not every `BeginFrame` will
  // result in submitting a new frame.
  virtual void DidSubmitCompositorFrame() = 0;

  // A frame submitted to GPU/viz has been processed. A frame will not begin
  // until a previous one has been ack-ed. This should generally happen after
  // every `DidSubmitCompositorFrame`, though there are edge cases such as
  // losing the frame sink.
  virtual void DidReceiveCompositorFrameAck() = 0;

  // Client should respond eventually by calling `LayerTree::SetFrameSink`.
  virtual void RequestNewFrameSink() = 0;

  // Calling `LayerTree::SetFrameSink` should eventually result in one of
  // these being called.
  virtual void DidInitializeLayerTreeFrameSink() = 0;
  virtual void DidFailToInitializeLayerTreeFrameSink() = 0;

  // Frame sink is lost. A new frame sink will be requested if needed
  // through `RequestNewFrameSink`.
  virtual void DidLoseLayerTreeFrameSink() = 0;
};

}  // namespace cc::slim

#endif  // CC_SLIM_LAYER_TREE_CLIENT_H_
