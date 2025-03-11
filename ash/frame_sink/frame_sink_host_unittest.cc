// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/frame_sink_host.h"

#include <memory>

#include "ash/frame_sink/frame_sink_holder.h"
#include "ash/frame_sink/frame_sink_holder_test_api.h"
#include "ash/frame_sink/test/frame_sink_host_test_base.h"
#include "ash/frame_sink/test/test_begin_frame_source.h"
#include "ash/frame_sink/test/test_frame_factory.h"
#include "ash/frame_sink/test/test_layer_tree_frame_sink.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class TestFrameSinkHost : public FrameSinkHost {
 public:
  TestFrameSinkHost() = default;

  TestFrameSinkHost(const TestFrameSinkHost&) = delete;
  TestFrameSinkHost& operator=(const TestFrameSinkHost&) = delete;

  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      UiResourceManager& resource_manager,
      bool auto_refresh,
      const gfx::Size& last_submitted_frame_size,
      float last_submitted_frame_dsf) override {
    return frame_factory_.CreateCompositorFrame(
        begin_frame_ack, resource_manager, auto_refresh,
        last_submitted_frame_size, last_submitted_frame_dsf);
  }

  TestFrameFactory& frame_factory() { return frame_factory_; }

  int on_first_frame_requested_counter() const {
    return on_first_frame_requested_counter_;
  }

  void OnFirstFrameRequested() override { ++on_first_frame_requested_counter_; }

 private:
  TestFrameFactory frame_factory_;
  int on_first_frame_requested_counter_ = 0;
};

using FrameSinkHostTest = FrameSinkHostTestBase<TestFrameSinkHost>;

TEST_F(FrameSinkHostTest, OnFirstFrameRequestedShouldOnlyBeCalledOnce) {
  EXPECT_EQ(frame_sink_host()->on_first_frame_requested_counter(), 0);

  // Request the first frame.
  OnBeginFrame();
  // `FrameSinkHost::OnFirstFrameRequested` should be called.
  EXPECT_EQ(frame_sink_host()->on_first_frame_requested_counter(), 1);

  // Request the second frame.
  OnBeginFrame();
  // `FrameSinkHost::OnFirstFrameRequested` should not be called again.
  EXPECT_EQ(frame_sink_host()->on_first_frame_requested_counter(), 1);
}

TEST_F(FrameSinkHostTest, OnFrameSinkLost) {
  FrameSinkHolderTestApi frame_sink_holder_test(
      frame_sink_host()->frame_sink_holder_for_testing());
  frame_sink_host()->frame_factory().SetFrameMetaData(
      /*frame_size=*/host_window()->bounds().size(), /*dsf=*/1.0f);
  EXPECT_EQ(frame_sinks_created_count(), 1);

  OnBeginFrame();
  EXPECT_TRUE(frame_sink_holder_test.IsFirstFrameRequested());
  EXPECT_EQ(layer_tree_frame_sink()->num_of_frames_received(), 0);

  frame_sink_host()->UpdateSurface(host_window()->bounds(),
                                   host_window()->bounds(),
                                   /*synchronous_draw=*/true);
  EXPECT_EQ(layer_tree_frame_sink()->num_of_frames_received(), 1);

  frame_sink_host()
      ->frame_sink_holder_for_testing()
      ->DidLoseLayerTreeFrameSink();

  // After losing a frame sink, a new frame sink is created and a new frame is
  // submitted to update the surface.
  EXPECT_EQ(frame_sinks_created_count(), 2);

  // We need to wait for first frame request before frame sink can submit a new
  // frame.
  OnBeginFrame();
  EXPECT_EQ(layer_tree_frame_sink()->num_of_frames_received(), 1);
}

}  // namespace
}  // namespace ash
