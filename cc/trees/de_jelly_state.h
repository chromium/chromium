// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_DE_JELLY_STATE_H_
#define CC_TREES_DE_JELLY_STATE_H_

#include <map>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "cc/cc_export.h"
#include "ui/gfx/transform.h"

namespace viz {
class SharedQuadState;
class RenderPass;
}  // namespace viz

namespace cc {
class LayerTreeImpl;

// Helper class which tracks the movement of layers and renderpasses
// and computes the |de_jelly_delta_y| for their SharedQuadState.
class CC_EXPORT DeJellyState {
 public:
  DeJellyState();
  ~DeJellyState();

  // Called once per frame to move tracking structure to the next frame and
  // determine if we should apply de-jelly at all.
  void AdvanceFrame(LayerTreeImpl* layer_tree_impl);

  // Populates |de_jelly_delta_y| for the most recent SharedQuadState on
  // |target_render_pass|.
  void UpdateSharedQuadState(LayerTreeImpl* layer_tree_impl,
                             int transform_id,
                             viz::RenderPass* target_render_pass);

 private:
  bool should_de_jelly_ = false;
  int scroll_transform_node_ = 0;
  float scroll_offset_ = 0;
  float fallback_delta_y_ = 0;

  base::Optional<gfx::Transform> new_scroll_node_transform_;
  std::map<int, gfx::Transform> previous_transforms_;
  std::map<int, gfx::Transform> new_transforms_;
};

}  // namespace cc

#endif  // CC_TREES_DE_JELLY_STATE_H_
