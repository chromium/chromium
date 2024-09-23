// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_FAST_INK_HOST_FRAME_UTILS_H_
#define ASH_FAST_INK_FAST_INK_HOST_FRAME_UTILS_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/frame_sink/ui_resource.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace gfx {
class Size;
}  // namespace gfx

namespace gpu {
class ClientSharedImage;
}

namespace aura {
class Window;
}  // namespace aura

namespace ash {
class UiResourceManager;

namespace fast_ink_internal {

inline constexpr viz::SharedImageFormat kFastInkSharedImageFormat =
    SK_B32_SHIFT ? viz::SinglePlaneFormat::kRGBA_8888
                 : viz::SinglePlaneFormat::kBGRA_8888;
inline constexpr UiSourceId kFastInkUiSourceId = 1u;

// Converts the rect in window's coordinate to the buffer's coordinate.  If the
// window is rotated, the window_rect will also be rotated, for example. The
// size is clamped by `buffer_size` to ensure it does not exceeds the buffer
// size.
ASH_EXPORT gfx::Rect BufferRectFromWindowRect(
    const gfx::Transform& window_to_buffer_transform,
    const gfx::Size& buffer_size,
    const gfx::Rect& window_rect);

// Creates a Mappable SharedImage of given `size`, `shared_image_usage`, and
// `buffer_usage`. The returned ClientSharedImage will be null if creation
// failed.
ASH_EXPORT scoped_refptr<gpu::ClientSharedImage> CreateMappableSharedImage(
    const gfx::Size& size,
    gpu::SharedImageUsageSet shared_image_usage,
    gfx::BufferUsage buffer_usage);

// Creates a UiResource of a given `size` and `format` using the SharedImage
// that `mailbox` (which must be non-zero) is referencing. The created
// UiResource does not own that SharedImage.
ASH_EXPORT std::unique_ptr<UiResource> CreateUiResource(
    const gfx::Size& size,
    UiSourceId ui_source_id,
    bool is_overlay_candidate,
    gpu::Mailbox mailbox,
    gpu::SyncToken sync_token);

// Creates and configures a compositor frame. Uses the SharedImage that
// `shared_image` (which must be non-null) is referencing. The created
// UiResource does not own that SharedImage.
ASH_EXPORT std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    const gfx::Rect& content_rect,
    const gfx::Rect& total_damage_rect,
    bool auto_update,
    const aura::Window& host_window,
    const gfx::Size& buffer_size,
    UiResourceManager* resource_manager,
    const scoped_refptr<gpu::ClientSharedImage>& shared_image,
    gpu::SyncToken sync_token);

// Returns the RasterContextProvider used within FastInk.
ASH_EXPORT scoped_refptr<viz::RasterContextProvider> GetContextProvider();

}  // namespace fast_ink_internal
}  // namespace ash

#endif  // ASH_FAST_INK_FAST_INK_HOST_FRAME_UTILS_H_
