// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_TREE_LAYER_LIST_DELEGATE_H_
#define CC_TREES_PROPERTY_TREE_LAYER_LIST_DELEGATE_H_

#include "cc/cc_export.h"
#include "cc/trees/property_tree_delegate.h"

namespace cc {

class LayerTreeHost;

// This is the default implementation of the PropertyTreeDelegate
// used when the LayerTreeHost is in layer list mode and the property
// trees are kept up to date directly by the LayerTreeHost's client.
// TODO(crbug.com/389771428): This class will be inlined back into
// the LayerTreeHost once the ui::Compositor has been updated to use a
// LayerTreeHost in layer list mode and we will no longer need multiple
// implementations of the PropertyTreeDelegate's logic.
class CC_EXPORT PropertyTreeLayerListDelegate : public PropertyTreeDelegate {
 public:
  // PropertyTreeDelegate overrides.
  void UpdatePropertyTreesIfNeeded(LayerTreeHost*) override;
};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_TREE_LAYER_LIST_DELEGATE_H_
