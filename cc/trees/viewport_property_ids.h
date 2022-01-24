// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_VIEWPORT_PROPERTY_IDS_H_
#define CC_TREES_VIEWPORT_PROPERTY_IDS_H_

#include "cc/paint/element_id.h"
#include "cc/trees/property_tree.h"

namespace cc {

struct ViewportPropertyIds {
  int overscroll_elasticity_transform = TransformTree::kInvalidNodeId;
  ElementId overscroll_elasticity_effect;
  int page_scale_transform = TransformTree::kInvalidNodeId;
  int inner_scroll = ScrollTree::kInvalidNodeId;
  int outer_clip = ClipTree::kInvalidNodeId;
  int outer_scroll = ScrollTree::kInvalidNodeId;
};

}  // namespace cc

#endif  // CC_TREES_VIEWPORT_PROPERTY_IDS_H_
