// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_TREE_LAYER_LIST_DELEGATE_H_
#define CC_TREES_PROPERTY_TREE_LAYER_LIST_DELEGATE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/paint/element_id.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/property_tree_delegate.h"

namespace gfx {
class Vector2dF;
class Transform;
}  // namespace gfx

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
  void SetLayerTreeHost(LayerTreeHost* host) override;
  LayerTreeHost* host() override;
  const LayerTreeHost* host() const override;
  void UpdatePropertyTreesIfNeeded() override;
  void UpdateScrollOffsetFromImpl(
      const ElementId& id,
      const gfx::Vector2dF& delta,
      const std::optional<TargetSnapAreaElementIds>& snap_target_ids) override;
  void OnAnimateLayers() override;
  void RegisterViewportPropertyIds(const ViewportPropertyIds& ids) override;
  void OnUnregisterElement(ElementId id) override;
  bool IsElementInPropertyTrees(ElementId element_id,
                                ElementListType list_type) const override;
  void OnElementFilterMutated(ElementId element_id,
                              ElementListType list_type,
                              const FilterOperations& filters) override;
  void OnElementBackdropFilterMutated(
      ElementId element_id,
      ElementListType list_type,
      const FilterOperations& backdrop_filters) override;
  void OnElementOpacityMutated(ElementId element_id,
                               ElementListType list_type,
                               float opacity) override;
  void OnElementTransformMutated(ElementId element_id,
                                 ElementListType list_type,
                                 const gfx::Transform& transform) override;

 private:
  raw_ptr<LayerTreeHost> host_ = nullptr;
};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_TREE_LAYER_LIST_DELEGATE_H_
