// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_host.h"

#include <algorithm>
#include <memory>

#include "ash/frame_sink/ui_resource_manager.h"
#include "ash/rounded_display/rounded_display_frame_factory.h"
#include "ash/rounded_display/rounded_display_gutter.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace ash {

RoundedDisplayHost::RoundedDisplayHost(GetGuttersCallback callback)
    : get_resource_generator_callback_(std::move(callback)),
      frame_factory_(std::make_unique<RoundedDisplayFrameFactory>()) {}

RoundedDisplayHost::~RoundedDisplayHost() = default;

std::unique_ptr<viz::CompositorFrame> RoundedDisplayHost::CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    UiResourceManager& resource_manager,
    bool auto_update,
    const gfx::Size& last_submitted_frame_size,
    float last_submitted_frame_dsf) {
  std::vector<RoundedDisplayGutter*> gutters;
  get_resource_generator_callback_.Run(gutters);

  auto frame = frame_factory_->CreateCompositorFrame(
      begin_frame_ack, *host_window(), resource_manager, gutters);

  // A change in the size of the compositor frame means we need to identify a
  // new surface to submit the compositor frame to since the surface size is now
  // different.
  if (last_submitted_frame_size != frame->size_in_pixels() ||
      last_submitted_frame_dsf != frame->device_scale_factor()) {
    host_window()->AllocateLocalSurfaceId();
  }

  return frame;
}

}  // namespace ash
