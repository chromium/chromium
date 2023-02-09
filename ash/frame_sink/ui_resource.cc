// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/ui_resource.h"

#include "gpu/command_buffer/client/shared_image_interface.h"

namespace ash {

UiResource::UiResource() = default;

UiResource::~UiResource() {
  if (!context_provider) {
    return;
  }

  gpu::SharedImageInterface* sii = context_provider->SharedImageInterface();
  DCHECK(!mailbox.IsZero());
  sii->DestroySharedImage(sync_token, mailbox);
}

}  // namespace ash
