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
  UiResource();

  UiResource(const UiResource&) = delete;
  UiResource& operator=(const UiResource&) = delete;

  virtual ~UiResource();

  // It is valid to call only one of the two below setters on a given instance,
  // and that setter should be called only once.
  void SetExternallyOwnedMailbox(const gpu::Mailbox& mailbox) {
    CHECK(external_mailbox_.IsZero());
    CHECK(!client_shared_image_);
    external_mailbox_ = mailbox;
  }
  void SetClientSharedImage(
      scoped_refptr<gpu::ClientSharedImage> client_shared_image) {
    CHECK(external_mailbox_.IsZero());
    CHECK(!client_shared_image_);
    client_shared_image_ = std::move(client_shared_image);
  }

  // Returns `client_shared_image_->mailbox()` if `client_shared_image_` has
  // been set and `external_mailbox_` otherwise.
  const gpu::Mailbox& mailbox() const {
    return client_shared_image_ ? client_shared_image_->mailbox()
                                : external_mailbox_;
  }

  const scoped_refptr<gpu::ClientSharedImage>& client_shared_image() const {
    return client_shared_image_;
  }

  scoped_refptr<viz::RasterContextProvider> context_provider;
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

 private:
  gpu::Mailbox external_mailbox_;
  scoped_refptr<gpu::ClientSharedImage> client_shared_image_;
};

}  // namespace ash

#endif  // ASH_FRAME_SINK_UI_RESOURCE_H_
