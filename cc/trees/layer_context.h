// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_CONTEXT_H_
#define CC_TREES_LAYER_CONTEXT_H_

#include "cc/cc_export.h"
#include "cc/trees/commit_state.h"
#include "components/viz/common/surfaces/local_surface_id.h"

namespace viz {
class ClientResourceProvider;
class RasterContextProvider;
}  // namespace viz

namespace cc {

class PictureLayerImpl;
class Tile;

// LayerContext provides an opaque interface through which a LayerTreeHost can
// control a backing LayerTreeHostImpl, potentially on another thread or in
// another process.
class CC_EXPORT LayerContext {
 public:
  virtual ~LayerContext() = default;

  // Globally controls the visibility of layers within the tree.
  virtual void SetVisible(bool visible) = 0;

  // Pushes updates from `tree` into the context's display tree.
  virtual void UpdateDisplayTreeFrom(
      LayerTreeImpl& tree,
      viz::ClientResourceProvider& resource_provider,
      viz::RasterContextProvider& context_provider) = 0;

  // Pushes an update to a single tile in the context's display tree.
  virtual void UpdateDisplayTile(
      PictureLayerImpl& layer,
      const Tile& tile,
      viz::ClientResourceProvider& resource_provider,
      viz::RasterContextProvider& context_provider) = 0;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_CONTEXT_H_
