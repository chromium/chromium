// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_FRAME_DATA_H_
#define CC_SLIM_FRAME_DATA_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "cc/base/simple_enclosed_region.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  viz::CompositorFrame& frame;
  std::vector<viz::HitTestRegion>& hit_test_regions;
  base::flat_set<viz::SurfaceId> activation_dependencies;
  absl::optional<uint32_t> deadline_in_frames;
  bool use_default_lower_bound_deadline = false;

  SimpleEnclosedRegion occlusion_in_target;
};

}  // namespace cc::slim

#endif  // CC_SLIM_FRAME_DATA_H_
