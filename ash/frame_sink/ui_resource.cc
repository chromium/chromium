// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/ui_resource.h"

#include "gpu/command_buffer/client/shared_image_interface.h"

namespace ash {

UiResource::UiResource(scoped_refptr<gpu::SharedImageInterface> sii)
    : shared_image_interface(std::move(sii)) {
  CHECK(shared_image_interface);
}

UiResource::~UiResource() {
  if (!client_shared_image_) {
    return;
  }

  shared_image_interface->DestroySharedImage(sync_token,
                                             std::move(client_shared_image_));
}

}  // namespace ash
