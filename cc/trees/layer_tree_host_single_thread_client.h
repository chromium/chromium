// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_SINGLE_THREAD_CLIENT_H_
#define CC_TREES_LAYER_TREE_HOST_SINGLE_THREAD_CLIENT_H_

#include "base/time/time.h"

namespace cc {

class LayerTreeHostSingleThreadClient {
 public:
  // Request that the client schedule a composite. For tests using single thread
  // without a scheduler.
  virtual void RequestScheduleComposite() {}

  // Called whenever the begin frame interval changes. This interval can be used
  // for animations.
  virtual void FrameIntervalUpdated(base::TimeDelta interval) {}

  // Called whenever the compositor submits a CompositorFrame. Afterward,
  // LayerTreeHostClient::DidReceiveCompositorFrameAck() will be called once the
  // display compositor/ finishes processing the frame. So these functions can
  // be used to keep track of pending submitted CompositorFrames for rate
  // limiting.
  virtual void DidSubmitCompositorFrame() = 0;

  // Called when the active LayerTreeFrameSink is lost and needs to be
  // replaced. This allows the embedder to schedule a composite which will
  // run the machinery to acquire a new LayerTreeFrameSink.
  virtual void DidLoseLayerTreeFrameSink() = 0;

 protected:
  virtual ~LayerTreeHostSingleThreadClient() {}
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_SINGLE_THREAD_CLIENT_H_
