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

  // Globally controls the visibility of layers within the tree.
  virtual void SetVisible(bool visible) = 0;

  // Pushes updates from `tree` into the the context's display tree.
  virtual void UpdateDisplayTreeFrom(LayerTreeImpl& tree) = 0;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_CONTEXT_H_
