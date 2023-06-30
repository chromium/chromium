// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_UI_RESOURCE_MANAGER_H_
#define ASH_FRAME_SINK_UI_RESOURCE_MANAGER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/frame_sink/ui_resource.h"
#include "base/containers/flat_map.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

// Responsible to keep track of resources that are currently in-use by display
// compositor and resources that are currently available to be reused.
// Once we offer a resource to the manager, UiResourceManager manages the
// lifetime of the resources and keep track if the resource is currently
// exported or is available to use. It is required that all the exported
// resources are reclaimed before we can delete the UiResourceManager.
class ASH_EXPORT UiResourceManager {
 public:
  UiResourceManager();

  UiResourceManager(const UiResourceManager&) = delete;
  UiResourceManager& operator=(const UiResourceManager&) = delete;

  ~UiResourceManager();

  // Returns the resource_id of an available resource of given `size`,
  // `format` and `ui_source_id`. If there is no matching resource available, we
  // return `viz::kInvalidResourceId`.
  viz::ResourceId FindResourceToReuse(const gfx::Size& size,
                                      viz::SharedImageFormat format,
                                      UiSourceId ui_source_id) const;

  const UiResource* PeekAvailableResource(viz::ResourceId resource_id) const;

  const UiResource* PeekExportedResource(viz::ResourceId resource_id) const;

  std::unique_ptr<UiResource> ReleaseAvailableResource(
      viz::ResourceId resource_id);

  void ReclaimResources(const std::vector<viz::ReturnedResource>& resources);

  // Give the `resourse` to be managed by the manager. The resource is
  // immediately available for use and can be exported.
  viz::ResourceId OfferResource(std::unique_ptr<UiResource> resource);

  // We take the available resource identified by `resource_id`, map it to a
  // transferable resource and mark it as an exported resource. Only a resource
  // that is currently being managed can be exported.
  viz::TransferableResource PrepareResourceForExport(
      viz::ResourceId resource_id);

  // Mark all the managed resources to be damaged.
  void DamageResources();

  void ClearAvailableResources();

  // Call the method when there is no way to reclaim back the exported
  // resources. i.e when the frame_sink is lost or deleted.
  void LostExportedResources();

  size_t exported_resources_count() const;

  size_t available_resources_count() const;

 private:
  // TODO(zoraiznaeem): If a feature ends up growing the size of pool past 40,
  // use a fixed size circular list as a pool and get rid of least recently used
  // resources.
  using ResourcePool =
      base::flat_map<viz::ResourceId, std::unique_ptr<UiResource>>;

  // The resources that are currently available to be used in a compositor
  // frame.
  ResourcePool available_resources_pool_;

  // The resources that are in-flight or will be exported to the display
  // compositor.
  ResourcePool exported_resources_pool_;

  // Generates transferable resource ids for transferable resources.
  viz::ResourceIdGenerator id_generator_;
};

}  // namespace ash

#endif  // ASH_FRAME_SINK_UI_RESOURCE_MANAGER_H_
