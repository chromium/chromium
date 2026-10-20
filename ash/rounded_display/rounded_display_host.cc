// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_host.h"

#include <algorithm>
#include <memory>

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
    viz::ClientResourceProvider& client_resource_provider,
    cc::ResourcePool& resource_pool,
    bool auto_update) {
  std::vector<RoundedDisplayGutter*> gutters;
  get_resource_generator_callback_.Run(gutters);

  return frame_factory_->CreateCompositorFrame(begin_frame_ack, *host_window(),
                                               client_resource_provider,
                                               resource_pool, gutters);
}

}  // namespace ash
