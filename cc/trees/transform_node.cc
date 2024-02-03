// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/transform_node.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/property_tree.h"
#include "ui/gfx/geometry/point3_f.h"

namespace cc {

TransformNode::TransformNode() = default;
TransformNode::TransformNode(const TransformNode&) = default;
TransformNode& TransformNode::operator=(const TransformNode&) = default;

#if DCHECK_IS_ON()
bool TransformNode::operator==(const TransformNode& other) const = default;
#endif  // DCHECK_IS_ON()

void TransformNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  element_id.AddToTracedValue(value);
  MathUtil::AddToTracedValue("local", local, value);
  MathUtil::AddToTracedValue("origin", origin, value);
  MathUtil::AddToTracedValue("post_translation", post_translation, value);
  value->SetInteger("sorting_context_id", sorting_context_id);
  value->SetBoolean("flattens_inherited_transform",
                    flattens_inherited_transform);
  value->SetBoolean("will_change_transform", will_change_transform);
  MathUtil::AddToTracedValue("scroll_offset", scroll_offset, value);
  MathUtil::AddToTracedValue("snap_amount", snap_amount, value);
}

TransformCachedNodeData::TransformCachedNodeData() = default;
TransformCachedNodeData::TransformCachedNodeData(
    const TransformCachedNodeData& other) = default;
TransformCachedNodeData::~TransformCachedNodeData() = default;
bool TransformCachedNodeData::operator==(
    const TransformCachedNodeData& other) const = default;

}  // namespace cc
