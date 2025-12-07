// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_TEST_TEST_FRAME_FACTORY_H_
#define ASH_FRAME_SINK_TEST_TEST_FRAME_FACTORY_H_

#include <vector>

#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/resources/resource_id.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
class CompositorFrame;
struct TransferableResource;
}  // namespace viz

namespace ash {
class UiResourceManager;

class TestFrameFactory {
 public:
  TestFrameFactory();

  TestFrameFactory(const TestFrameFactory&) = delete;
  TestFrameFactory& operator=(const TestFrameFactory&) = delete;

  ~TestFrameFactory();

  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      UiResourceManager& resource_manager,
      bool auto_refresh,
      const gfx::Size& last_submitted_frame_size,
      float last_submitted_frame_dsf);

  void SetFrameResources(
      std::vector<viz::TransferableResource> frame_resources);
  void SetFrameMetaData(const gfx::Size& frame_size, float dsf);

 private:
  std::vector<viz::TransferableResource> latest_frame_resources_;
  gfx::Size latest_frame_size_;
  float latest_frame_dsf_ = 1.0;
};

}  // namespace ash

#endif  // ASH_FRAME_SINK_TEST_TEST_FRAME_FACTORY_H_
