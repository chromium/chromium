// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_APPEND_QUADS_DATA_H_
#define CC_LAYERS_APPEND_QUADS_DATA_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "cc/cc_export.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace cc {

// Set by the layer appending quads.
class CC_EXPORT AppendQuadsData {
 public:
  AppendQuadsData();
  ~AppendQuadsData();

  int num_missing_tiles = 0;

  bool checkerboarded_needs_raster = false;
  bool checkerboarded_needs_record = false;

  int64_t visible_layer_area = 0;

  // The visible content area of tiles that are of low or non-ideal resolution.
  int64_t approximated_visible_content_area = 0;

  // The non-default number of BeginFrames to wait before forcibly activating
  // this CompositorFrame.
  std::optional<uint32_t> deadline_in_frames;

  // Indicates whether or not one of the layers wants to use the default
  // activation deadline.
  bool use_default_lower_bound_deadline = false;

  // This is the set of surface IDs that must have corresponding
  // active CompositorFrames so that this CompositorFrame can
  // activate.
  std::vector<viz::SurfaceId> activation_dependencies;

  // Indicates if any layer has ViewTransitionElementResourceIds which need to
  // be swapped with actual viz::ResourceIds in the Viz process.
  bool has_shared_element_resources = false;
};

}  // namespace cc
#endif  // CC_LAYERS_APPEND_QUADS_DATA_H_
