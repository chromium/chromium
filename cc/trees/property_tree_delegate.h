// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_TREE_DELEGATE_H_
#define CC_TREES_PROPERTY_TREE_DELEGATE_H_

#include <optional>

#include "cc/input/scroll_snap_data.h"
#include "cc/paint/element_id.h"
#include "cc/trees/mutator_host_client.h"

namespace gfx {
class Vector2dF;
class Transform;
}  // namespace gfx

namespace cc {

class LayerTreeHost;
struct ViewportPropertyIds;

// This is an interface that LayerTreeHosts and LayerTreeHostClients can
// implement to control how the host's property trees are kept up to date.
// There are two default implementations, in PropertyTreeLayerListDelegate and
// PropertyTreeLayerTreeDelegate, depending on whether the LayerTreeHost is in
// layer list mode or not, but the client can also provide its own delegate via
// the `LayerTreeHost::InitParams.property_tree_delegate` field.
// TODO(crbug.com/389771428): This will eventually be removed once the
// ui::Compositor has been updated to use a LayerTreeHost in layer list mode.
// The multiple implementations will no longer be needed and the
// PropertyTreeLayerListDelegate's implementation can be merged back into the
// LayerTreeHost (if so desired).
class PropertyTreeDelegate {
 public:
  virtual ~PropertyTreeDelegate() = default;

  virtual void SetLayerTreeHost(LayerTreeHost* host) = 0;
  virtual LayerTreeHost* host() = 0;
  virtual const LayerTreeHost* host() const = 0;

  // Called by LayerTreeHost::DoUpdateLayers() to ensure that the
  // property trees are up-to-date.
  virtual void UpdatePropertyTreesIfNeeded() = 0;

  virtual void UpdateScrollOffsetFromImpl(
      const ElementId& id,
      const gfx::Vector2dF& delta,
      const std::optional<TargetSnapAreaElementIds>& snap_target_ids) = 0;

  virtual void OnAnimateLayers() = 0;

  virtual void RegisterViewportPropertyIds(const ViewportPropertyIds& ids) = 0;

  virtual void OnUnregisterElement(ElementId id) = 0;

  virtual bool IsElementInPropertyTrees(ElementId element_id,
                                        ElementListType list_type) const = 0;

  virtual void OnElementFilterMutated(ElementId element_id,
                                      ElementListType list_type,
                                      const FilterOperations& filters) = 0;

  virtual void OnElementBackdropFilterMutated(
      ElementId element_id,
      ElementListType list_type,
      const FilterOperations& backdrop_filters) = 0;

  virtual void OnElementOpacityMutated(ElementId element_id,
                                       ElementListType list_type,
                                       float opacity) = 0;

  virtual void OnElementTransformMutated(ElementId element_id,
                                         ElementListType list_type,
                                         const gfx::Transform& transform) = 0;
};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_TREE_DELEGATE_H_
