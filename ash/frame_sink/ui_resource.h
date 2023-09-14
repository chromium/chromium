// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_UI_RESOURCE_H_
#define ASH_FRAME_SINK_UI_RESOURCE_H_

#include "ash/ash_export.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format.h"
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
  UiResource();

  UiResource(const UiResource&) = delete;
  UiResource& operator=(const UiResource&) = delete;

  virtual ~UiResource();

  scoped_refptr<viz::RasterContextProvider> context_provider;
  gpu::Mailbox mailbox;
  gpu::SyncToken sync_token;
  viz::SharedImageFormat format;
  gfx::Size resource_size;

  // This id can be used to identify the resource back to the type of source
  // generating the resourse. It must be a non-zero number.
  UiSourceId ui_source_id = kInvalidUiSourceId;

  // Unique id to identify the resource.
  viz::ResourceId resource_id = viz::kInvalidResourceId;

  // If the candidate should be promoted to use hw overlays.
  bool is_overlay_candidate = false;

  // If the textures represented by the resource is damaged.
  bool damaged = true;
};

}  // namespace ash

#endif  // ASH_FRAME_SINK_UI_RESOURCE_H_
