// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_RENDER_FRAME_METADATA_OBSERVER_H_
#define CC_TREES_RENDER_FRAME_METADATA_OBSERVER_H_

#include "cc/cc_export.h"
#include "cc/trees/render_frame_metadata.h"

namespace viz {
class CompositorFrameMetadata;
}

namespace cc {

// Observes RenderFrameMetadata associated with the submission of a frame.
// LayerTreeHostImpl will create the metadata when submitting a CompositorFrame.
//
// Calls to this will be done from the compositor thread.
class CC_EXPORT RenderFrameMetadataObserver {
 public:
  RenderFrameMetadataObserver() = default;
  RenderFrameMetadataObserver(const RenderFrameMetadataObserver&) = delete;
  virtual ~RenderFrameMetadataObserver() = default;

  RenderFrameMetadataObserver& operator=(const RenderFrameMetadataObserver&) =
      delete;

  // Binds on the current thread. This should only be called from the compositor
  // thread.
  virtual void BindToCurrentThread() = 0;

  // Notification of the RendarFrameMetadata for the frame being submitted to
  // the display compositor.
  virtual void OnRenderFrameSubmission(
      const RenderFrameMetadata& render_frame_metadata,
      viz::CompositorFrameMetadata* compositor_frame_metadata,
      bool force_send) = 0;
};

}  // namespace cc

#endif  // CC_TREES_RENDER_FRAME_METADATA_OBSERVER_H_
