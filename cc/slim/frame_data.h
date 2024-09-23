// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_FRAME_DATA_H_
#define CC_SLIM_FRAME_DATA_H_

#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/slim/damage_data.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "ui/gfx/geometry/mask_filter_info.h"

namespace viz {
class CompositorFrame;
struct HitTestRegion;
}  // namespace viz

namespace cc::slim {

// Modifiable data passed to AppendQuads during tree walk.
struct FrameData {
  FrameData(viz::CompositorFrame& frame,
            std::vector<viz::HitTestRegion>& regions);
  ~FrameData();

  const raw_ref<viz::CompositorFrame> frame;
  const raw_ref<std::vector<viz::HitTestRegion>> hit_test_regions;
  base::flat_set<viz::SurfaceId> activation_dependencies;
  std::optional<uint32_t> deadline_in_frames;
  bool use_default_lower_bound_deadline = false;

  // These fields are for a particular render pass (ie target) and the
  // recursive tree walk will update and clear these fields for new
  // render passes as needed
  SimpleEnclosedRegion occlusion_in_target;
  RenderPassDamageData render_pass_damage;
  gfx::MaskFilterInfo mask_filter_info_in_target;
  viz::OffsetTag offset_tag;

  FrameDamageData current_frame_damage;
  bool subtree_property_changed_from_parent = false;
};

}  // namespace cc::slim

#endif  // CC_SLIM_FRAME_DATA_H_
