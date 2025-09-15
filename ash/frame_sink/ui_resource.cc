// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/ui_resource.h"

#include "gpu/command_buffer/client/shared_image_interface.h"

namespace ash {

UiResource::UiResource(scoped_refptr<gpu::SharedImageInterface> sii,
                       scoped_refptr<gpu::ClientSharedImage> shared_image)
    : shared_image_interface(std::move(sii)),
      client_shared_image_(std::move(shared_image)) {
  CHECK(shared_image_interface);
  CHECK(client_shared_image_);
}

UiResource::~UiResource() {
  client_shared_image_->UpdateDestructionSyncToken(sync_token);
}

}  // namespace ash
