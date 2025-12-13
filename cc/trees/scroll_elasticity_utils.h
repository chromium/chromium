// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SCROLL_ELASTICITY_UTILS_H_
#define CC_TREES_SCROLL_ELASTICITY_UTILS_H_

#include "cc/cc_export.h"

namespace cc {
struct ScrollNode;
class ScrollTree;
class LayerTreeHostImpl;

namespace scroll_elasticity_utils {

bool CC_EXPORT ShouldAllowOverscrollEffect(const ScrollNode& node,
                                           const ScrollTree& scroll_tree,
                                           const LayerTreeHostImpl* host_impl);
}
}  // namespace cc

#endif  // CC_TREES_SCROLL_ELASTICITY_UTILS_H_
