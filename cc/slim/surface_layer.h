// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_SURFACE_LAYER_H_
#define CC_SLIM_SURFACE_LAYER_H_

#include <optional>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "cc/layers/deadline_policy.h"
#include "cc/slim/layer.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"

namespace cc::slim {

// A layer that embeds content from another viz client.
class COMPONENT_EXPORT(CC_SLIM) SurfaceLayer : public Layer {
 public:
  static scoped_refptr<SurfaceLayer> Create();

  const viz::SurfaceId& surface_id() const { return surface_range_.end(); }

  // Set the surface id that this layer is embedding. `deadline_policy`
  // specifies behavior and timeout for how long to wait for the surface to be
  // ready to draw before giving up.
  void SetSurfaceId(const viz::SurfaceId& surface_id,
                    const cc::DeadlinePolicy& deadline_policy);

  // When stretch_content_to_fill_bounds is true, the scale of the embedded
  // surface is ignored and the content will be stretched to fill the bounds.
  void SetStretchContentToFillBounds(bool stretch_content_to_fill_bounds);
  bool stretch_content_to_fill_bounds() const {
    return stretch_content_to_fill_bounds_;
  }

  // Set the oldest surface id that can be used as fallback assuming current
  // surface being embedded isn't ready to be drawn yet (before first frame is
  // submitted).
  void SetOldestAcceptableFallback(const viz::SurfaceId& surface_id);
  const std::optional<viz::SurfaceId>& oldest_acceptable_fallback() const {
    return surface_range_.start();
  }

  // Registers `tag` so it can be used to tag layers to be moved by the display
  // compositor. `constraints` provides limits to the possible offset. This
  // should be called to register `tag` before any layers are tagged with it.
  // It can be called repeatedly with the same tag to update constraints.
  //
  // When the display compositor draws it will lookup the OffsetTagValue from
  // the same viz::Surface this surface layer embeds.
  void RegisterOffsetTag(const viz::OffsetTag& tag,
                         const viz::OffsetTagConstraints& constraints);
  // Unregisters `tag`. Layers should have `tag` removed before unregistering.
  // This must be called only once for a registered tag.
  void UnregisterOffsetTag(const viz::OffsetTag& tag);

  void SetLayerTree(LayerTree* layer_tree) override;

  // Internal methods called by LayerTreeImpl.
  viz::OffsetTagDefinition GetOffsetTagDefinition(const viz::OffsetTag& tag);

 private:
  SurfaceLayer();
  ~SurfaceLayer() override;

  void SetSurfaceRange(const viz::SurfaceRange& surface_range);

  // Layer implementation.
  void AppendQuads(viz::CompositorRenderPass& render_pass,
                   FrameData& data,
                   const gfx::Transform& transform_to_root,
                   const gfx::Transform& transform_to_target,
                   const gfx::Rect* clip_in_target,
                   const gfx::Rect& visible_rect,
                   float opacity) override;

  bool stretch_content_to_fill_bounds_ = false;
  viz::SurfaceRange surface_range_;
  std::optional<uint32_t> deadline_in_frames_;
  base::flat_map<viz::OffsetTag, viz::OffsetTagConstraints> offset_tags_;
};

}  // namespace cc::slim

#endif  // CC_SLIM_SURFACE_LAYER_H_
