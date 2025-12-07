// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/test/test_frame_factory.h"

#include "ash/frame_sink/ui_resource_manager.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

TestFrameFactory::TestFrameFactory() = default;

TestFrameFactory::~TestFrameFactory() = default;

std::unique_ptr<viz::CompositorFrame> TestFrameFactory::CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    UiResourceManager& resource_manager,
    bool auto_refresh,
    const gfx::Size& last_submitted_frame_size,
    float last_submitted_frame_dsf) {
  auto frame = std::make_unique<viz::CompositorFrame>();

  frame->metadata.begin_frame_ack = begin_frame_ack;
  frame->metadata.device_scale_factor = latest_frame_dsf_;

  const viz::CompositorRenderPassId kRenderPassId{1};
  auto render_pass = viz::CompositorRenderPass::Create();
  render_pass->SetNew(kRenderPassId, gfx::Rect(latest_frame_size_), gfx::Rect(),
                      gfx::Transform());

  frame->render_pass_list.push_back(std::move(render_pass));
  frame->resource_list = latest_frame_resources_;

  return frame;
}

void TestFrameFactory::SetFrameResources(
    std::vector<viz::TransferableResource> frame_resources) {
  latest_frame_resources_ = std::move(frame_resources);
}

void TestFrameFactory::SetFrameMetaData(const gfx::Size& frame_size,
                                        float dsf) {
  latest_frame_size_ = frame_size;
  latest_frame_dsf_ = dsf;
}

}  // namespace ash
