// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_CONTEXT_H_
#define CC_TREES_LAYER_CONTEXT_H_

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/trees/commit_state.h"
#include "components/viz/common/surfaces/local_surface_id.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace gpu {
class SharedImageInterface;
}  // namespace gpu

namespace viz {
class ClientResourceProvider;
class LocalSurfaceId;
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
  virtual base::TimeTicks UpdateDisplayTreeFrom(
      LayerTreeImpl& tree,
      viz::ClientResourceProvider& resource_provider,
      gpu::SharedImageInterface* shared_image_interface,
      const gfx::Rect& viewport_damage_rect,
      const viz::LocalSurfaceId& target_local_surface_id,
      bool frame_has_damage) = 0;

  // Pushes an update to a single tile in the context's display tree.
  virtual void UpdateDisplayTile(
      PictureLayerImpl& layer,
      const Tile& tile,
      viz::ClientResourceProvider& resource_provider,
      gpu::SharedImageInterface* shared_image_interface,
      bool update_damage) = 0;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_CONTEXT_H_
