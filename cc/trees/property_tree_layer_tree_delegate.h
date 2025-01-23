// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_TREE_LAYER_TREE_DELEGATE_H_
#define CC_TREES_PROPERTY_TREE_LAYER_TREE_DELEGATE_H_

#include "cc/cc_export.h"
#include "cc/trees/property_tree_delegate.h"

namespace cc {

class LayerTreeHost;

// This is the default implementation of PropertyTreeDelegate that
// is used when the LayerTreeHost is in the legacy layer tree mode.
// TODO(crbug.com/389771428): This will eventually be removed once the
// ui::Compositor has been updated to use a LayerTreeHost in layer list mode.
// The multiple implementations of PropertyTreeDelegate will no longer be
// needed and the PropertyTreeLayerListDelegate's implementation can be
// merged back into the LayerTreeHost (if so desired).
class CC_EXPORT PropertyTreeLayerTreeDelegate : public PropertyTreeDelegate {
 public:
  // PropertyTreeDelegate overrides.
  void UpdatePropertyTreesIfNeeded(LayerTreeHost*) override;
};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_TREE_LAYER_TREE_DELEGATE_H_
