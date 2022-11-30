// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_TREE_BUILDER_H_
#define CC_TREES_PROPERTY_TREE_BUILDER_H_

#include "cc/cc_export.h"

namespace cc {

class LayerTreeHost;

class PropertyTreeBuilder {
 public:
  static void CC_EXPORT BuildPropertyTrees(LayerTreeHost*);
};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_TREE_BUILDER_H_
