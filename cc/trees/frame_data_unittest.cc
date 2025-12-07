// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/frame_data.h"

#include <utility>

#include "base/test/trace_test_utils.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_log.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/trees_in_viz_timing.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

TEST(FrameDataTest, FrameDataAsValueTest) {
  base::test::TracingEnvironment tracing_env;
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
                                     ""));
  FrameData frame;
  auto begin_frame_args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1));
  frame.begin_frame_ack = viz::BeginFrameAck(begin_frame_args, true);
  frame.origin_begin_main_frame_args = begin_frame_args;

  // Create one fake render pass
  auto render_pass = viz::CompositorRenderPass::Create();
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  gfx::Size content_bounds;
  gfx::Rect quad_rect(content_bounds);
  auto* quad = render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(shared_quad_state, quad_rect, quad_rect, SkColors::kTransparent,
               false);

  viz::CompositorRenderPassList render_pass_list;
  render_pass_list.push_back(std::move(render_pass));
  frame.render_passes = std::move(render_pass_list);
  std::string frame_string = frame.ToString();

  // Test that the frame has some strings set.
  EXPECT_TRUE(base::Contains(frame_string, ("\"has_no_damage\": false")));
  EXPECT_TRUE(base::Contains(frame_string, ("\"render_passes\": [ {")));

  // Disable tracelog to avoid teardown failures.
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
}

TEST(FrameDataTest, FrameDataSetTreesInVizTimestamps) {
  FrameData frame;
  viz::TreesInVizTiming timing_details{
      base::TimeTicks::Now(), base::TimeTicks::Now() + base::Milliseconds(1),
      base::TimeTicks::Now() + base::Milliseconds(2),
      base::TimeTicks::Now() + base::Milliseconds(3)};
  frame.set_trees_in_viz_timestamps(timing_details);

  ASSERT_EQ(frame.trees_in_viz_timing_details->start_update_display_tree,
            timing_details.start_update_display_tree);
  ASSERT_EQ(frame.trees_in_viz_timing_details->start_prepare_to_draw,
            timing_details.start_prepare_to_draw);
  ASSERT_EQ(frame.trees_in_viz_timing_details->start_draw_layers,
            timing_details.start_draw_layers);
  ASSERT_EQ(frame.trees_in_viz_timing_details->submit_compositor_frame,
            timing_details.submit_compositor_frame);
}

}  // namespace cc
