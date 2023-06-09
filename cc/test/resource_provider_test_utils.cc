// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/resource_provider_test_utils.h"

#include <unordered_map>
#include <vector>

#include "base/functional/callback_helpers.h"

namespace cc {

const std::
    unordered_map<viz::ResourceId, viz::ResourceId, viz::ResourceIdHasher>&
    SendResourceAndGetChildToParentMap(
        const std::vector<viz::ResourceId>& resource_ids,
        viz::DisplayResourceProvider* resource_provider,
        viz::ClientResourceProvider* child_resource_provider,
        viz::RasterContextProvider* child_context_provider) {
  DCHECK(resource_provider);
  DCHECK(child_resource_provider);
  // Transfer resources to the parent.
  std::vector<viz::TransferableResource> send_to_parent;
  int child_id =
      resource_provider->CreateChild(base::DoNothing(), viz::SurfaceId());
  child_resource_provider->PrepareSendToParent(resource_ids, &send_to_parent,
                                               child_context_provider);
  resource_provider->ReceiveFromChild(child_id, send_to_parent);

  // Delete them in the child so they won't be leaked, and will be released once
  // returned from the parent. This assumes they won't need to be sent to the
  // parent again.
  for (viz::ResourceId id : resource_ids)
    child_resource_provider->RemoveImportedResource(id);

  // Return the child to parent map.
  return resource_provider->GetChildToParentMap(child_id);
}

}  // namespace cc
