// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_CONTEXT_CLIENT_H_
#define CC_TREES_LAYER_CONTEXT_CLIENT_H_

#include "cc/cc_export.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

// Interface used by LayerContext to push requests to its corresponding
// client-side LayerTreeHost.
class CC_EXPORT LayerContextClient {
 public:
  virtual ~LayerContextClient() = default;

  // Indicates that the compositor will produce a new display frame soon and
  // that the client should commit a new layer tree ASAP. `args` correspond to
  // the impending display frame that the compositor wants to produce.
  virtual void OnRequestCommitForFrame(const viz::BeginFrameArgs& args) = 0;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_CONTEXT_CLIENT_H_
