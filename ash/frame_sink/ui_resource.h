// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_UI_RESOURCE_H_
#define ASH_FRAME_SINK_UI_RESOURCE_H_

#include "ash/ash_export.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

using UiSourceId = uint32_t;
inline constexpr UiSourceId kInvalidUiSourceId(0u);

// A handle to a texture resource. Contains the meta data and context that can
// map the resource back to the source of the resource.
class ASH_EXPORT UiResource {
 public:
  UiResource(scoped_refptr<gpu::SharedImageInterface> sii,
             scoped_refptr<gpu::ClientSharedImage> shared_image);

  UiResource(const UiResource&) = delete;
  UiResource& operator=(const UiResource&) = delete;

  virtual ~UiResource();

  const scoped_refptr<gpu::ClientSharedImage>& client_shared_image() const {
    return client_shared_image_;
  }

  scoped_refptr<gpu::SharedImageInterface> shared_image_interface;
  gpu::SyncToken sync_token;
  gfx::Size resource_size;

  // This id can be used to identify the resource back to the type of source
  // generating the resourse. It must be a non-zero number.
  UiSourceId ui_source_id = kInvalidUiSourceId;

  // If the candidate should be promoted to use hw overlays.
  bool is_overlay_candidate = false;

  // If the textures represented by the resource is damaged.
  bool damaged = true;

 private:
  scoped_refptr<gpu::ClientSharedImage> client_shared_image_;
};

}  // namespace ash

#endif  // ASH_FRAME_SINK_UI_RESOURCE_H_
