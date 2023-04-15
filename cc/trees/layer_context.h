// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_CONTEXT_H_
#define CC_TREES_LAYER_CONTEXT_H_

#include "cc/cc_export.h"
#include "cc/trees/commit_state.h"
#include "components/viz/common/surfaces/local_surface_id.h"

namespace cc {

// LayerContext provides an opaque interface through which a LayerTreeHost can
// control a backing LayerTreeHostImpl, potentially on another thread or in
// another process.
class CC_EXPORT LayerContext {
 public:
  virtual ~LayerContext() = default;

  // Indicates that a new LocalSurfaceId has been set for the frame sink hosting
  // this tree.
  virtual void SetTargetLocalSurfaceId(const viz::LocalSurfaceId& id) = 0;

  // Globally controls the visibility of layers within the tree.
  virtual void SetVisible(bool visible) = 0;

  // Flushes pending updates to the backing LayerTreeHostImpl. `state`
  // represents the pending CommitState for the client-side LayerTreeHost.
  virtual void Commit(const CommitState& state) = 0;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_CONTEXT_H_
