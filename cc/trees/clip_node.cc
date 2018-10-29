// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/clip_node.h"

#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/trees/property_tree.h"

#include "base/trace_event/traced_value.h"

namespace cc {

ClipNode::ClipNode()
    : id(ClipTree::kInvalidNodeId),
      parent_id(ClipTree::kInvalidNodeId),
      clip_type(ClipType::APPLIES_LOCAL_CLIP),
      transform_id(TransformTree::kInvalidNodeId) {
}

ClipNode::ClipNode(const ClipNode& other) = default;

ClipNode& ClipNode::operator=(const ClipNode& other) = default;

ClipNode::~ClipNode() = default;

bool ClipNode::operator==(const ClipNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         clip_type == other.clip_type && clip == other.clip &&
         clip_expander == other.clip_expander &&
         transform_id == other.transform_id;
}

void ClipNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("clip_type", static_cast<int>(clip_type));
  MathUtil::AddToTracedValue("clip", clip, value);
  value->SetInteger("transform_id", transform_id);
}

}  // namespace cc
