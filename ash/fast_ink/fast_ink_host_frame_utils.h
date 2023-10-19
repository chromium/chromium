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
#include "ui/gfx/geometry/rect.h"

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace gfx {
class GpuMemoryBuffer;
class Size;
}  // namespace gfx

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

// Creates a gpu buffer of given `size` and of given usage and format defined in
// `usage_and_format`.
ASH_EXPORT std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuBuffer(
    const gfx::Size& size,
    const gfx::BufferUsageAndFormat& usage_and_format);

// Creates a UiResource of a given `size` and `format`. Uses the SharedImage
// that `mailbox` is referencing if that is non-zero, in which case the created
// UiResource does not own that SharedImage. Otherwise creates a new SharedImage
// and has the UiResource take ownership of that SharedImage.
ASH_EXPORT std::unique_ptr<UiResource> CreateUiResource(
    const gfx::Size& size,
    UiSourceId ui_source_id,
    bool is_overlay_candidate,
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    gpu::Mailbox mailbox,
    gpu::SyncToken sync_token);

// Creates and configures a compositor frame. Uses the SharedImage that
// `mailbox` is referencing if that is non-zero, in which case the created
// UiResource does not own that SharedImage. Otherwise creates a new SharedImage
// if needing to create a new UiResource and has the UiResource take ownership
// of that SharedImage.
ASH_EXPORT std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    const gfx::Rect& content_rect,
    const gfx::Rect& total_damage_rect,
    bool auto_update,
    const aura::Window& host_window,
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    UiResourceManager* resource_manager,
    gpu::Mailbox mailbox,
    gpu::SyncToken sync_token);

}  // namespace fast_ink_internal
}  // namespace ash

#endif  // ASH_FAST_INK_FAST_INK_HOST_FRAME_UTILS_H_
