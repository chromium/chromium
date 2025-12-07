// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer_impl.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/layer_test_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_host_impl_test_base.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class TreesInVizServerLayerTreeHostImplTest : public LayerTreeHostImplTest {
 public:
  LayerTreeSettings DefaultSettings() override {
    LayerTreeSettings settings = LayerTreeHostImplTest::DefaultSettings();
    settings.trees_in_viz_in_viz_process = true;
    return settings;
  }
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(TreesInVizServerLayerTreeHostImplTest);

// [TreesInViz] Tests that frame data timestamps get to CompositorFrameMetadata,
// this behaviour is only valid when layer tree steps occur in Viz.
TEST_P(TreesInVizServerLayerTreeHostImplTest,
       FrameDataTimestampsGetSetInCFMetadata) {
  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));

  // Make a child layer that draws.
  auto* layer = AddLayer<SolidColorLayerImpl>(host_impl_->active_tree());
  layer->SetBounds(gfx::Size(10, 10));
  layer->SetDrawsContent(true);
  layer->SetBackgroundColor(SkColors::kRed);
  CopyProperties(root, layer);

  UpdateDrawProperties(host_impl_->active_tree());
  TestFrameData frame;
  frame.set_trees_in_viz_timestamps(
      {base::TimeTicks::Now(), base::TimeTicks::Now() + base::Milliseconds(1),
       base::TimeTicks::Now() + base::Milliseconds(2),
       base::TimeTicks::Now() + base::Milliseconds(3)});
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1));
  host_impl_->WillBeginImplFrame(args);
  // This would be set by LayerContextImpl as part of UpdateDisplayTree, set
  // manually to avoid DCHECK failure.
  host_impl_->set_next_frame_token_from_client(frame.frame_token + 1);
  EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));

  // This function sets the metadata timestamps from FrameData.
  std::optional<SubmitInfo> submit_info = host_impl_->DrawLayers(&frame);

  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());
  const viz::CompositorFrameMetadata& metadata =
      fake_layer_tree_frame_sink->last_sent_frame()->metadata;

  // Asset that the timestamps are assigned as expected.
  EXPECT_EQ(frame.trees_in_viz_timing_details->start_update_display_tree,
            metadata.trees_in_viz_timing_details.start_update_display_tree);
  EXPECT_EQ(frame.trees_in_viz_timing_details->start_prepare_to_draw,
            metadata.trees_in_viz_timing_details.start_prepare_to_draw);
  EXPECT_EQ(frame.trees_in_viz_timing_details->start_draw_layers,
            metadata.trees_in_viz_timing_details.start_draw_layers);
  // This timestamp is set inside DrawLayers, so it should be
  // equat to submit info submit time.
  EXPECT_EQ(submit_info.value().time,
            metadata.trees_in_viz_timing_details.submit_compositor_frame);
}

}  // namespace cc
