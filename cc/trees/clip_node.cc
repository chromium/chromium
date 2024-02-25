// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/clip_node.h"

#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/trees/property_tree.h"

namespace cc {

ClipNode::ClipNode() = default;
ClipNode::ClipNode(const ClipNode& other) = default;
ClipNode& ClipNode::operator=(const ClipNode& other) = default;
ClipNode::~ClipNode() = default;

bool ClipNode::AppliesLocalClip() const {
  return pixel_moving_filter_id == kInvalidPropertyNodeId;
}

#if DCHECK_IS_ON()
bool ClipNode::operator==(const ClipNode& other) const {
  return id == other.id && parent_id == other.parent_id && clip == other.clip &&
         pixel_moving_filter_id == other.pixel_moving_filter_id &&
         transform_id == other.transform_id;
}
#endif

void ClipNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  MathUtil::AddToTracedValue("clip", clip, value);
  value->SetInteger("pixel_moving_filter_id", pixel_moving_filter_id);
  value->SetInteger("transform_id", transform_id);
}

}  // namespace cc
