// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "cc/base/region.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/filter_operations.h"
#include "cc/slim/layer.h"
#include "cc/slim/nine_patch_layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/surface_layer.h"
#include "cc/slim/test_frame_sink_impl.h"
#include "cc/slim/test_layer_tree_client.h"
#include "cc/slim/test_layer_tree_impl.h"
#include "cc/slim/ui_resource_layer.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/test/draw_quad_matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc::slim {

namespace {

using testing::AllOf;
using testing::ElementsAre;

class SlimLayerTreeCompositorFrameTest : public testing::Test {
 public:
  void SetUp() override {
    layer_tree_ = std::make_unique<TestLayerTreeImpl>(&client_);
    layer_tree_->SetVisible(true);

    auto frame_sink = TestFrameSinkImpl::Create();
    frame_sink_ = frame_sink->GetWeakPtr();
    layer_tree_->SetFrameSink(std::move(frame_sink));

    viewport_ = gfx::Rect(100, 100);
    base::UnguessableToken token = base::UnguessableToken::Create();
    local_surface_id_ = viz::LocalSurfaceId(1u, 2u, token);
    EXPECT_TRUE(local_surface_id_.is_valid());
    layer_tree_->SetViewportRectAndScale(
        viewport_, /*device_scale_factor=*/1.0f, local_surface_id_);
  }

  void IncrementLocalSurfaceId() {
    DCHECK(local_surface_id_.is_valid());
    local_surface_id_ =
        viz::LocalSurfaceId(local_surface_id_.parent_sequence_number(),
                            local_surface_id_.child_sequence_number() + 1,
                            local_surface_id_.embed_token());
    DCHECK(local_surface_id_.is_valid());
  }

  viz::CompositorFrame ProduceFrame(
      std::optional<viz::HitTestRegionList>* out_list = nullptr) {
    layer_tree_->SetNeedsAnimate();
    EXPECT_TRUE(layer_tree_->NeedsBeginFrames());
    base::TimeTicks frame_time = base::TimeTicks::Now();
    base::TimeDelta interval = viz::BeginFrameArgs::DefaultInterval();
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE,
        /*source_id=*/1, ++sequence_id_, frame_time, frame_time + interval,
        interval, viz::BeginFrameArgs::NORMAL);
    frame_sink_->OnBeginFrame(begin_frame_args, std::move(next_timing_details_),
                              /*frame_ack=*/false, {});
    next_timing_details_.clear();
    viz::CompositorFrame frame = frame_sink_->TakeLastFrame();
    if (out_list) {
      *out_list = frame_sink_->GetLastHitTestRegionList();
    }
    frame_sink_->DidReceiveCompositorFrameAck({});
    return frame;
  }

  scoped_refptr<SolidColorLayer> CreateSolidColorLayer(const gfx::Size& bounds,
                                                       SkColor4f color) {
    auto solid_color_layer = SolidColorLayer::Create();
    solid_color_layer->SetBounds(bounds);
    solid_color_layer->SetBackgroundColor(color);
    solid_color_layer->SetIsDrawable(true);
    return solid_color_layer;
  }

  void SetNextFrameTimingDetailsMap(viz::FrameTimingDetailsMap timing_map) {
    next_timing_details_ = std::move(timing_map);
  }

  viz::FrameTimingDetails BuildFrameTimingDetails(uint32_t flags = 0) {
    viz::FrameTimingDetails details;
    base::TimeTicks timestamp = base::TimeTicks::Now();
    base::TimeDelta interval = base::Milliseconds(16.6);
    gfx::PresentationFeedback feedback(timestamp, interval, flags);
    details.presentation_feedback = feedback;
    return details;
  }

 protected:
  TestLayerTreeClient client_;
  std::unique_ptr<TestLayerTreeImpl> layer_tree_;
  base::WeakPtr<TestFrameSinkImpl> frame_sink_;

  uint64_t sequence_id_ = 0;
  viz::FrameTimingDetailsMap next_timing_details_;

  gfx::Rect viewport_;
  viz::LocalSurfaceId local_surface_id_;
};

TEST_F(SlimLayerTreeCompositorFrameTest, CompositorFrameMetadataBasics) {
  auto solid_color_layer =
      CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(solid_color_layer);

  uint32_t first_frame_token = 0u;
  {
    viz::CompositorFrame frame = ProduceFrame();
    viz::CompositorFrameMetadata& metadata = frame.metadata;
    EXPECT_NE(0u, metadata.frame_token);
    first_frame_token = metadata.frame_token;
    EXPECT_EQ(sequence_id_, metadata.begin_frame_ack.frame_id.sequence_number);
    EXPECT_EQ(1.0f, metadata.device_scale_factor);
    EXPECT_EQ(SkColors::kWhite, metadata.root_background_color);
    EXPECT_EQ(gfx::OVERLAY_TRANSFORM_NONE, metadata.display_transform_hint);
  }

  IncrementLocalSurfaceId();
  layer_tree_->SetViewportRectAndScale(viewport_, /*device_scale_factor=*/2.0f,
                                       local_surface_id_);
  layer_tree_->set_background_color(SkColors::kBlue);
  layer_tree_->set_display_transform_hint(
      gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90);
  {
    viz::CompositorFrame frame = ProduceFrame();
    viz::CompositorFrameMetadata& metadata = frame.metadata;
    EXPECT_NE(0u, metadata.frame_token);
    EXPECT_NE(first_frame_token, metadata.frame_token);
    EXPECT_EQ(sequence_id_, metadata.begin_frame_ack.frame_id.sequence_number);
    EXPECT_EQ(2.0f, metadata.device_scale_factor);
    EXPECT_EQ(SkColors::kBlue, metadata.root_background_color);
    EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
              metadata.display_transform_hint);
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, OneSolidColorQuad) {
  auto solid_color_layer =
      CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(solid_color_layer);

  viz::CompositorFrame frame = ProduceFrame();

  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  EXPECT_EQ(pass->output_rect, viewport_);
  EXPECT_EQ(pass->damage_rect, viewport_);
  EXPECT_EQ(pass->transform_to_root_target, gfx::Transform());

  ASSERT_THAT(
      pass->quad_list,
      ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                        viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                        viz::HasTransform(gfx::Transform()),
                        viz::HasOpacity(1.0f), viz::AreContentsOpaque(true))));
  auto* quad = pass->quad_list.back();
  auto* shared_quad_state = quad->shared_quad_state;

  EXPECT_EQ(shared_quad_state->quad_layer_rect, viewport_);
  EXPECT_EQ(shared_quad_state->visible_quad_layer_rect, viewport_);
  EXPECT_EQ(shared_quad_state->clip_rect, std::nullopt);
  EXPECT_EQ(shared_quad_state->are_contents_opaque, true);
  EXPECT_EQ(shared_quad_state->blend_mode, SkBlendMode::kSrcOver);
}

TEST_F(SlimLayerTreeCompositorFrameTest, LayerTransform) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto child = CreateSolidColorLayer(gfx::Size(10, 20), SkColors::kGreen);
  root_layer->AddChild(child);

  auto check_child_quad = [&](gfx::Rect expected_rect_in_root) {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    ASSERT_THAT(pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kGreen),
                                  viz::HasRect(gfx::Rect(10, 20)),
                                  viz::HasVisibleRect(gfx::Rect(10, 20))),
                            AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                                  viz::HasRect(viewport_),
                                  viz::HasVisibleRect(viewport_))));

    auto* quad = pass->quad_list.front();
    auto* shared_quad_state = quad->shared_quad_state;

    EXPECT_EQ(shared_quad_state->quad_layer_rect, gfx::Rect(10, 20));
    EXPECT_EQ(shared_quad_state->visible_quad_layer_rect, gfx::Rect(10, 20));

    gfx::Rect rect_in_root =
        shared_quad_state->quad_to_target_transform.MapRect(quad->rect);
    EXPECT_EQ(expected_rect_in_root, rect_in_root);
  };

  child->SetPosition(gfx::PointF(30.0f, 30.0f));
  check_child_quad(gfx::Rect(30, 30, 10, 20));

  child->SetTransform(gfx::Transform::MakeTranslation(10.0f, 10.0f));
  check_child_quad(gfx::Rect(40, 40, 10, 20));

  // Rotate about top left corner.
  child->SetTransform(gfx::Transform::Make90degRotation());
  check_child_quad(gfx::Rect(10, 30, 20, 10));

  // Rotate about the center.
  child->SetTransformOrigin(gfx::PointF(5.0f, 10.0f));
  check_child_quad(gfx::Rect(25, 35, 20, 10));
}

TEST_F(SlimLayerTreeCompositorFrameTest, ChildOrder) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  scoped_refptr<SolidColorLayer> children[] = {
      CreateSolidColorLayer(gfx::Size(10, 10), SkColors::kBlue),
      CreateSolidColorLayer(gfx::Size(10, 10), SkColors::kGreen),
      CreateSolidColorLayer(gfx::Size(10, 10), SkColors::kMagenta),
      CreateSolidColorLayer(gfx::Size(10, 10), SkColors::kRed),
      CreateSolidColorLayer(gfx::Size(10, 10), SkColors::kYellow)};

  // Build tree such that quads appear in child order.
  // Quads are appended post order depth first, in reverse child order.
  // root <- child4 <- child3
  //                <- child2
  //      <- child1 <- child0
  root_layer->AddChild(children[4]);
  root_layer->AddChild(children[1]);
  children[4]->AddChild(children[3]);
  children[4]->AddChild(children[2]);
  children[1]->AddChild(children[0]);

  // Add offsets so they do not cover each other.
  children[3]->SetPosition(gfx::PointF(10.0f, 10.0f));
  children[2]->SetPosition(gfx::PointF(20.0f, 20.0f));
  children[1]->SetPosition(gfx::PointF(30.0f, 30.0f));
  children[0]->SetPosition(gfx::PointF(10.0f, 10.0f));

  gfx::Point expected_origins[] = {
      gfx::Point(40.0f, 40.0f), gfx::Point(30.0f, 30.0f),
      gfx::Point(20.0f, 20.0f), gfx::Point(10.0f, 10.0f),
      gfx::Point(00.0f, 00.0f)};

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  ASSERT_THAT(pass->quad_list,
              ElementsAre(viz::IsSolidColorQuad(SkColors::kBlue),
                          viz::IsSolidColorQuad(SkColors::kGreen),
                          viz::IsSolidColorQuad(SkColors::kMagenta),
                          viz::IsSolidColorQuad(SkColors::kRed),
                          viz::IsSolidColorQuad(SkColors::kYellow),
                          viz::IsSolidColorQuad(SkColors::kGray)));

  for (size_t i = 0; i < std::size(expected_origins); ++i) {
    auto* quad = pass->quad_list.ElementAt(i);
    EXPECT_EQ(quad->shared_quad_state->quad_to_target_transform.MapPoint(
                  gfx::Point()),
              expected_origins[i]);
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, AxisAlignedClip) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto clip_layer = Layer::Create();
  clip_layer->SetBounds(gfx::Size(10, 20));
  clip_layer->SetMasksToBounds(true);

  auto draw_layer = CreateSolidColorLayer(gfx::Size(30, 30), SkColors::kRed);

  root_layer->AddChild(clip_layer);
  clip_layer->AddChild(draw_layer);

  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    ASSERT_THAT(pass->quad_list,
                ElementsAre(viz::IsSolidColorQuad(SkColors::kRed),
                            viz::IsSolidColorQuad(SkColors::kGray)));

    auto* quad = pass->quad_list.front();
    ASSERT_TRUE(quad->shared_quad_state->clip_rect);
    EXPECT_EQ(quad->shared_quad_state->clip_rect.value(), gfx::Rect(10, 20));
  }

  clip_layer->SetPosition(gfx::PointF(5, 5));
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    ASSERT_THAT(pass->quad_list,
                ElementsAre(viz::IsSolidColorQuad(SkColors::kRed),
                            viz::IsSolidColorQuad(SkColors::kGray)));

    auto* quad = pass->quad_list.front();
    ASSERT_TRUE(quad->shared_quad_state->clip_rect);
    // Clip is in target space.
    EXPECT_EQ(quad->shared_quad_state->clip_rect.value(),
              gfx::Rect(5, 5, 10, 20));
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, PresentationCallback) {
  auto solid_color_layer =
      CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(solid_color_layer);

  std::optional<gfx::PresentationFeedback> feedback_opt_1;
  std::optional<gfx::PresentationFeedback> feedback_opt_2;
  layer_tree_->RequestPresentationTimeForNextFrame(base::BindLambdaForTesting(
      [&](const gfx::PresentationFeedback& feedback) {
        feedback_opt_1 = feedback;
      }));
  layer_tree_->RequestPresentationTimeForNextFrame(base::BindLambdaForTesting(
      [&](const gfx::PresentationFeedback& feedback) {
        feedback_opt_2 = feedback;
      }));
  viz::CompositorFrame frame1 = ProduceFrame();

  viz::FrameTimingDetailsMap timing_map;
  viz::FrameTimingDetails details = BuildFrameTimingDetails();
  timing_map[frame1.metadata.frame_token] = details;
  SetNextFrameTimingDetailsMap(std::move(timing_map));
  viz::CompositorFrame frame2 = ProduceFrame();

  ASSERT_TRUE(feedback_opt_1);
  ASSERT_TRUE(feedback_opt_2);
  EXPECT_EQ(feedback_opt_1.value(), details.presentation_feedback);
  EXPECT_EQ(feedback_opt_2.value(), details.presentation_feedback);
}

TEST_F(SlimLayerTreeCompositorFrameTest, PresentationCallbackMissedFrame) {
  auto solid_color_layer =
      CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(solid_color_layer);

  std::optional<gfx::PresentationFeedback> feedback_opt_1;
  layer_tree_->RequestPresentationTimeForNextFrame(base::BindLambdaForTesting(
      [&](const gfx::PresentationFeedback& feedback) {
        feedback_opt_1 = feedback;
      }));
  viz::CompositorFrame frame1 = ProduceFrame();

  std::optional<gfx::PresentationFeedback> feedback_opt_2;
  layer_tree_->RequestPresentationTimeForNextFrame(base::BindLambdaForTesting(
      [&](const gfx::PresentationFeedback& feedback) {
        feedback_opt_2 = feedback;
      }));
  viz::CompositorFrame frame2 = ProduceFrame();
  viz::CompositorFrame frame3 = ProduceFrame();
  EXPECT_FALSE(feedback_opt_1);
  EXPECT_FALSE(feedback_opt_2);

  {
    // Ack frame 1 which should only run the first callback.
    viz::FrameTimingDetailsMap timing_map;
    viz::FrameTimingDetails details = BuildFrameTimingDetails();
    timing_map[frame1.metadata.frame_token] = details;
    SetNextFrameTimingDetailsMap(std::move(timing_map));
    viz::CompositorFrame frame4 = ProduceFrame();

    EXPECT_TRUE(feedback_opt_1);
    EXPECT_EQ(feedback_opt_1.value(), details.presentation_feedback);
    EXPECT_FALSE(feedback_opt_2);
  }

  {
    // Ack frame 3, skipping frame 2, which should only run the second callback.
    viz::FrameTimingDetailsMap timing_map;
    viz::FrameTimingDetails details = BuildFrameTimingDetails();
    timing_map[frame3.metadata.frame_token] = details;
    SetNextFrameTimingDetailsMap(std::move(timing_map));
    viz::CompositorFrame frame4 = ProduceFrame();

    ASSERT_TRUE(feedback_opt_2);
    EXPECT_EQ(feedback_opt_2.value(), details.presentation_feedback);
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, SuccessPresentationCallback) {
  auto solid_color_layer =
      CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(solid_color_layer);

  std::optional<base::TimeTicks> feedback_time_opt_1;
  std::optional<base::TimeTicks> feedback_time_opt_2;
  layer_tree_->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting([&](const viz::FrameTimingDetails& details) {
        feedback_time_opt_1 = details.presentation_feedback.timestamp;
      }));
  layer_tree_->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting([&](const viz::FrameTimingDetails& details) {
        feedback_time_opt_2 = details.presentation_feedback.timestamp;
      }));
  viz::CompositorFrame frame1 = ProduceFrame();

  viz::FrameTimingDetailsMap timing_map;
  viz::FrameTimingDetails details = BuildFrameTimingDetails();
  timing_map[frame1.metadata.frame_token] = details;
  SetNextFrameTimingDetailsMap(std::move(timing_map));
  viz::CompositorFrame frame2 = ProduceFrame();

  ASSERT_TRUE(feedback_time_opt_1);
  ASSERT_TRUE(feedback_time_opt_2);
  EXPECT_EQ(feedback_time_opt_1.value(),
            details.presentation_feedback.timestamp);
  EXPECT_EQ(feedback_time_opt_2.value(),
            details.presentation_feedback.timestamp);
}

TEST_F(SlimLayerTreeCompositorFrameTest,
       SuccessPresentationCallbackNotCalledForFailedFrame) {
  auto solid_color_layer =
      CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(solid_color_layer);

  std::optional<base::TimeTicks> feedback_time_opt_1;
  layer_tree_->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting([&](const viz::FrameTimingDetails& details) {
        feedback_time_opt_1 = details.presentation_feedback.timestamp;
      }));
  viz::CompositorFrame frame1 = ProduceFrame();
  viz::CompositorFrame frame2 = ProduceFrame();

  std::optional<base::TimeTicks> feedback_time_opt_2;
  layer_tree_->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting([&](const viz::FrameTimingDetails& details) {
        feedback_time_opt_2 = details.presentation_feedback.timestamp;
      }));
  viz::CompositorFrame frame3 = ProduceFrame();

  // Frame 1 failed. Should not run either callback.
  {
    viz::FrameTimingDetailsMap timing_map;
    viz::FrameTimingDetails details =
        BuildFrameTimingDetails(gfx::PresentationFeedback::kFailure);
    timing_map[frame1.metadata.frame_token] = details;
    SetNextFrameTimingDetailsMap(std::move(timing_map));
    viz::CompositorFrame frame4 = ProduceFrame();
    EXPECT_FALSE(feedback_time_opt_1);
    EXPECT_FALSE(feedback_time_opt_2);
  }

  // Successful feedback for frame 2. Should run callback 1 but not 2.
  {
    viz::FrameTimingDetailsMap timing_map;
    viz::FrameTimingDetails details = BuildFrameTimingDetails();
    timing_map[frame2.metadata.frame_token] = details;
    SetNextFrameTimingDetailsMap(std::move(timing_map));
    viz::CompositorFrame frame5 = ProduceFrame();
    ASSERT_TRUE(feedback_time_opt_1);
    EXPECT_EQ(feedback_time_opt_1.value(),
              details.presentation_feedback.timestamp);
    ASSERT_FALSE(feedback_time_opt_2);
  }

  // Successful feedback for frame 3. Should run 2.
  {
    viz::FrameTimingDetailsMap timing_map;
    viz::FrameTimingDetails details = BuildFrameTimingDetails();
    timing_map[frame3.metadata.frame_token] = details;
    SetNextFrameTimingDetailsMap(std::move(timing_map));
    viz::CompositorFrame frame5 = ProduceFrame();
    ASSERT_TRUE(feedback_time_opt_2);
    EXPECT_EQ(feedback_time_opt_2.value(),
              details.presentation_feedback.timestamp);
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, CopyOutputRequest) {
  auto solid_color_layer =
      CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(solid_color_layer);
  {
    viz::CompositorFrame frame = ProduceFrame();
    EXPECT_FALSE(layer_tree_->NeedsBeginFrames());
  }

  auto copy_request_no_source_1 = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::DoNothing());
  auto copy_request_no_source_2 = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::DoNothing());

  base::UnguessableToken token = base::UnguessableToken::Create();
  auto copy_request_with_source = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::DoNothing());
  copy_request_with_source->set_source(token);
  auto copy_request_with_same_source = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::DoNothing());
  copy_request_with_same_source->set_source(token);

  base::UnguessableToken token2 = base::UnguessableToken::Create();
  auto copy_request_with_difference_source =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kSystemMemory,
          base::DoNothing());
  copy_request_with_difference_source->set_source(token2);

  layer_tree_->RequestCopyOfOutput(std::move(copy_request_no_source_1));
  EXPECT_TRUE(layer_tree_->NeedsBeginFrames());
  layer_tree_->RequestCopyOfOutput(std::move(copy_request_no_source_2));
  layer_tree_->RequestCopyOfOutput(std::move(copy_request_with_source));
  layer_tree_->RequestCopyOfOutput(std::move(copy_request_with_same_source));
  layer_tree_->RequestCopyOfOutput(
      std::move(copy_request_with_difference_source));

  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    ASSERT_EQ(pass->copy_requests.size(), 4u);
    EXPECT_TRUE(pass->copy_requests[0]);
    EXPECT_TRUE(pass->copy_requests[1]);
    EXPECT_TRUE(pass->copy_requests[2]);
    EXPECT_TRUE(pass->copy_requests[3]);
  }

  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    ASSERT_EQ(pass->copy_requests.size(), 0u);
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, UIResourceLayerAppendQuads) {
  auto ui_resource_layer = UIResourceLayer::Create();
  ui_resource_layer->SetBounds(viewport_.size());
  ui_resource_layer->SetIsDrawable(true);
  ui_resource_layer->SetContentsOpaque(true);
  layer_tree_->SetRoot(ui_resource_layer);

  viz::ResourceId first_resource_id = viz::kInvalidResourceId;
  {
    auto image_info =
        SkImageInfo::Make(1, 1, kN32_SkColorType, kPremul_SkAlphaType);
    SkBitmap bitmap;
    bitmap.allocPixels(image_info);
    bitmap.setImmutable();
    ui_resource_layer->SetBitmap(bitmap);

    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    ASSERT_THAT(pass->quad_list,
                ElementsAre(AllOf(viz::IsTextureQuad(), viz::HasRect(viewport_),
                                  viz::HasVisibleRect(viewport_),
                                  viz::HasTransform(gfx::Transform()))));
    const viz::TextureDrawQuad* texture_quad =
        viz::TextureDrawQuad::MaterialCast(pass->quad_list.front());
    EXPECT_TRUE(texture_quad->needs_blending);
    EXPECT_NE(viz::kInvalidResourceId, texture_quad->resource_id());
    EXPECT_EQ(gfx::PointF(0.0f, 0.0f), texture_quad->uv_top_left);
    EXPECT_EQ(gfx::PointF(1.0f, 1.0f), texture_quad->uv_bottom_right);

    ASSERT_EQ(frame.resource_list.size(), 1u);
    EXPECT_EQ(frame.resource_list[0].id, texture_quad->resource_id());
    EXPECT_EQ(frame.resource_list[0].size, gfx::Size(1, 1));
    first_resource_id = texture_quad->resource_id();

    ASSERT_EQ(frame_sink_->uploaded_resources().size(), 1u);
    EXPECT_EQ(frame_sink_->uploaded_resources().begin()->second.viz_resource_id,
              texture_quad->resource_id());
  }

  ui_resource_layer->SetUV(gfx::PointF(0.25f, 0.25f),
                           gfx::PointF(0.75f, 0.75f));
  {
    auto image_info =
        SkImageInfo::Make(2, 2, kN32_SkColorType, kPremul_SkAlphaType);
    SkBitmap bitmap;
    bitmap.allocPixels(image_info);
    bitmap.setImmutable();
    ui_resource_layer->SetBitmap(bitmap);

    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    ASSERT_THAT(pass->quad_list,
                ElementsAre(AllOf(viz::IsTextureQuad(), viz::HasRect(viewport_),
                                  viz::HasVisibleRect(viewport_),
                                  viz::HasTransform(gfx::Transform()))));
    const viz::TextureDrawQuad* texture_quad =
        viz::TextureDrawQuad::MaterialCast(pass->quad_list.front());
    EXPECT_TRUE(texture_quad->needs_blending);
    EXPECT_NE(viz::kInvalidResourceId, texture_quad->resource_id());
    EXPECT_EQ(gfx::PointF(0.25f, 0.25f), texture_quad->uv_top_left);
    EXPECT_EQ(gfx::PointF(0.75f, 0.75f), texture_quad->uv_bottom_right);

    ASSERT_EQ(frame.resource_list.size(), 1u);
    EXPECT_EQ(frame.resource_list[0].id, texture_quad->resource_id());
    EXPECT_EQ(frame.resource_list[0].size, gfx::Size(2, 2));
    EXPECT_NE(first_resource_id, texture_quad->resource_id());
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, ReclaimResources) {
  constexpr size_t kNumLayers = 6;
  std::vector<scoped_refptr<UIResourceLayer>> layers;
  for (size_t i = 0; i < kNumLayers; ++i) {
    layers.push_back(UIResourceLayer::Create());
    layers[i]->SetBounds(viewport_.size());
    layers[i]->SetIsDrawable(true);
    if (i == 0u) {
      layer_tree_->SetRoot(layers[i]);
    } else {
      layers[i - 1]->AddChild(layers[i]);
    }

    auto image_info =
        SkImageInfo::Make(1, 1, kN32_SkColorType, kPremul_SkAlphaType);
    SkBitmap bitmap;
    bitmap.allocPixels(image_info);
    bitmap.setImmutable();
    layers[i]->SetBitmap(bitmap);
  }

  viz::CompositorFrame frame = ProduceFrame();
  EXPECT_EQ(frame.resource_list.size(), kNumLayers);
  for (size_t i = 0; i < kNumLayers; ++i) {
    EXPECT_TRUE(frame_sink_->client_resource_provider()->InUseByConsumer(
        frame.resource_list[i].id));
  }

  // Return every other resource.
  std::vector<viz::ReturnedResource> returned_resources;
  for (size_t i = 0; i < kNumLayers; i += 2) {
    returned_resources.push_back(frame.resource_list[i].ToReturnedResource());
  }
  frame_sink_->ReclaimResources(std::move(returned_resources));
  for (size_t i = 0; i < kNumLayers; i += 2) {
    EXPECT_FALSE(frame_sink_->client_resource_provider()->InUseByConsumer(
        frame.resource_list[i].id));
  }
  for (size_t i = 1; i < kNumLayers; i += 2) {
    EXPECT_TRUE(frame_sink_->client_resource_provider()->InUseByConsumer(
        frame.resource_list[i].id));
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, NinePatchLayerAppendQuads) {
  auto nine_patch_layer = NinePatchLayer::Create();
  nine_patch_layer->SetBounds(viewport_.size());
  nine_patch_layer->SetIsDrawable(true);
  nine_patch_layer->SetContentsOpaque(true);
  layer_tree_->SetRoot(nine_patch_layer);

  auto image_info =
      SkImageInfo::Make(10, 10, kN32_SkColorType, kPremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.allocPixels(image_info);
  bitmap.setImmutable();
  nine_patch_layer->SetBitmap(bitmap);

  nine_patch_layer->SetBorder(gfx::Rect(10, 10, 20, 20));  // 10 pixel border.
  nine_patch_layer->SetAperture(gfx::Rect(2, 2, 6, 6));
  nine_patch_layer->SetFillCenter(true);
  nine_patch_layer->SetNearestNeighbor(true);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.resource_list.size(), 1u);
  EXPECT_EQ(frame.resource_list[0].size, gfx::Size(10, 10));
  ASSERT_EQ(frame_sink_->uploaded_resources().size(), 1u);
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  ASSERT_THAT(
      pass->quad_list,
      ElementsAre(
          // Top left.
          AllOf(viz::IsTextureQuad(), viz::HasRect(gfx::Rect(10, 10)),
                viz::HasVisibleRect(gfx::Rect(10, 10))),
          // Top right.
          AllOf(viz::IsTextureQuad(), viz::HasRect(gfx::Rect(90, 0, 10, 10)),
                viz::HasVisibleRect(gfx::Rect(90, 0, 10, 10))),
          // Bottom left.
          AllOf(viz::IsTextureQuad(), viz::HasRect(gfx::Rect(0, 90, 10, 10)),
                viz::HasVisibleRect(gfx::Rect(0, 90, 10, 10))),
          // Bottom right.
          AllOf(viz::IsTextureQuad(), viz::HasRect(gfx::Rect(90, 90, 10, 10)),
                viz::HasVisibleRect(gfx::Rect(90, 90, 10, 10))),
          // Top.
          AllOf(viz::IsTextureQuad(), viz::HasRect(gfx::Rect(10, 0, 80, 10)),
                viz::HasVisibleRect(gfx::Rect(10, 0, 80, 10))),
          // Left.
          AllOf(viz::IsTextureQuad(), viz::HasRect(gfx::Rect(0, 10, 10, 80)),
                viz::HasVisibleRect(gfx::Rect(0, 10, 10, 80))),
          // Right.
          AllOf(viz::IsTextureQuad(), viz::HasRect(gfx::Rect(90, 10, 10, 80)),
                viz::HasVisibleRect(gfx::Rect(90, 10, 10, 80))),
          // Bottom.
          AllOf(viz::IsTextureQuad(), viz::HasRect(gfx::Rect(10, 90, 80, 10)),
                viz::HasVisibleRect(gfx::Rect(10, 90, 80, 10))),
          // Center.
          AllOf(viz::IsTextureQuad(),
                viz::HasRect(gfx::Rect(10, 10, 80, 80)))));
  gfx::PointF expected_uv_top_left[] = {
      gfx::PointF(0.0f, 0.0f),  // Top left.
      gfx::PointF(0.8f, 0.0f),  // Top right.
      gfx::PointF(0.0f, 0.8f),  // Bottom left.
      gfx::PointF(0.8f, 0.8f),  // Bottom right.
      gfx::PointF(0.2f, 0.0f),  // Top.
      gfx::PointF(0.0f, 0.2f),  // Left.
      gfx::PointF(0.8f, 0.2f),  // Right.
      gfx::PointF(0.2f, 0.8f),  // Bottom.
      gfx::PointF(0.2f, 0.2f),  // Center.
  };
  gfx::PointF expected_uv_bottom_right[] = {
      gfx::PointF(0.2f, 0.2f),  // Top left.
      gfx::PointF(1.0f, 0.2f),  // Top right.
      gfx::PointF(0.2f, 1.0f),  // Bottom left.
      gfx::PointF(1.0f, 1.0f),  // Bottom right.
      gfx::PointF(0.8f, 0.2f),  // Top.
      gfx::PointF(0.2f, 0.8f),  // Left.
      gfx::PointF(1.0f, 0.8f),  // Right.
      gfx::PointF(0.8f, 1.0f),  // Bottom.
      gfx::PointF(0.8f, 0.8f),  // Center.
  };
  for (size_t i = 0; i < std::size(expected_uv_top_left); ++i) {
    const viz::TextureDrawQuad* texture_quad =
        viz::TextureDrawQuad::MaterialCast(pass->quad_list.ElementAt(i));
    EXPECT_NE(viz::kInvalidResourceId, texture_quad->resource_id());
    EXPECT_TRUE(texture_quad->nearest_neighbor);
    EXPECT_EQ(expected_uv_top_left[i], texture_quad->uv_top_left);
    EXPECT_EQ(expected_uv_bottom_right[i], texture_quad->uv_bottom_right);

    EXPECT_EQ(frame.resource_list[0].id, texture_quad->resource_id());
    EXPECT_EQ(frame_sink_->uploaded_resources().begin()->second.viz_resource_id,
              texture_quad->resource_id());
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, SurfaceLayerAppendQuads) {
  auto surface_layer = SurfaceLayer::Create();
  surface_layer->SetBounds(viewport_.size());
  surface_layer->SetIsDrawable(true);
  surface_layer->SetContentsOpaque(true);
  layer_tree_->SetRoot(surface_layer);

  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    ASSERT_THAT(
        pass->quad_list,
        ElementsAre(AllOf(viz::IsSolidColorQuad(), viz::HasRect(viewport_),
                          viz::HasVisibleRect(viewport_))));
  }

  base::UnguessableToken token = base::UnguessableToken::Create();
  viz::SurfaceId start(viz::FrameSinkId(1u, 2u),
                       viz::LocalSurfaceId(3u, 4u, token));
  {
    viz::SurfaceId end(viz::FrameSinkId(1u, 2u),
                       viz::LocalSurfaceId(5u, 6u, token));
    cc::DeadlinePolicy deadline_policy =
        cc::DeadlinePolicy::UseDefaultDeadline();
    surface_layer->SetOldestAcceptableFallback(start);
    surface_layer->SetSurfaceId(end, deadline_policy);

    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    ASSERT_THAT(pass->quad_list,
                ElementsAre(AllOf(viz::IsSurfaceQuad(), viz::HasRect(viewport_),
                                  viz::HasVisibleRect(viewport_))));

    auto* quad = viz::SurfaceDrawQuad::MaterialCast(pass->quad_list.back());
    EXPECT_EQ(quad->surface_range, viz::SurfaceRange(start, end));
    EXPECT_FALSE(quad->stretch_content_to_fill_bounds);
    EXPECT_FALSE(quad->is_reflection);
    EXPECT_TRUE(quad->allow_merge);

    viz::CompositorFrameMetadata& metadata = frame.metadata;
    EXPECT_EQ(metadata.referenced_surfaces,
              std::vector<viz::SurfaceRange>{viz::SurfaceRange(start, end)});
    EXPECT_EQ(metadata.activation_dependencies,
              std::vector<viz::SurfaceId>{end});
    EXPECT_FALSE(metadata.deadline.deadline_in_frames());
    EXPECT_TRUE(metadata.deadline.use_default_lower_bound_deadline());
  }

  {
    viz::SurfaceId end(viz::FrameSinkId(1u, 2u),
                       viz::LocalSurfaceId(5u, 7u, token));
    cc::DeadlinePolicy deadline_policy =
        cc::DeadlinePolicy::UseSpecifiedDeadline(2u);
    surface_layer->SetSurfaceId(end, deadline_policy);
    surface_layer->SetStretchContentToFillBounds(true);

    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    ASSERT_THAT(pass->quad_list,
                ElementsAre(AllOf(viz::IsSurfaceQuad(), viz::HasRect(viewport_),
                                  viz::HasVisibleRect(viewport_))));

    auto* quad = viz::SurfaceDrawQuad::MaterialCast(pass->quad_list.back());
    EXPECT_EQ(quad->surface_range, viz::SurfaceRange(start, end));
    EXPECT_TRUE(quad->stretch_content_to_fill_bounds);

    viz::CompositorFrameMetadata& metadata = frame.metadata;
    EXPECT_EQ(metadata.referenced_surfaces,
              std::vector<viz::SurfaceRange>{viz::SurfaceRange(start, end)});
    EXPECT_EQ(metadata.activation_dependencies,
              std::vector<viz::SurfaceId>{end});
    EXPECT_EQ(metadata.deadline.deadline_in_frames(), 2u);
    EXPECT_FALSE(metadata.deadline.use_default_lower_bound_deadline());
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, SimpleHitTestRegionList) {
  auto surface_layer = SurfaceLayer::Create();
  surface_layer->SetBounds(viewport_.size());
  surface_layer->SetIsDrawable(true);
  layer_tree_->SetRoot(surface_layer);

  {
    base::UnguessableToken token = base::UnguessableToken::Create();
    viz::SurfaceId surface_id(viz::FrameSinkId(1u, 2u),
                              viz::LocalSurfaceId(3u, 4u, token));
    cc::DeadlinePolicy deadline_policy =
        cc::DeadlinePolicy::UseDefaultDeadline();
    surface_layer->SetSurfaceId(surface_id, deadline_policy);

    std::optional<viz::HitTestRegionList> hit_test_region_list;
    viz::CompositorFrame frame = ProduceFrame(&hit_test_region_list);
    ASSERT_TRUE(hit_test_region_list);
    EXPECT_EQ(hit_test_region_list->bounds, viewport_);

    ASSERT_EQ(hit_test_region_list->regions.size(), 1u);
    auto& hit_test_region = hit_test_region_list->regions.front();
    EXPECT_EQ(hit_test_region.frame_sink_id, viz::FrameSinkId(1u, 2u));
    EXPECT_EQ(hit_test_region.rect, viewport_);
    EXPECT_EQ(hit_test_region.transform, gfx::Transform());
  }

  auto child_surface_layer = SurfaceLayer::Create();
  surface_layer->AddChild(child_surface_layer);
  child_surface_layer->SetBounds(gfx::Size(10, 10));
  child_surface_layer->SetIsDrawable(true);
  child_surface_layer->SetPosition(gfx::PointF(10.0f, 10.0f));
  child_surface_layer->SetTransformOrigin(gfx::PointF(5.0f, 5.0f));
  gfx::Transform transform;
  transform.Rotate(45.0);
  child_surface_layer->SetTransform(transform);

  base::UnguessableToken token = base::UnguessableToken::Create();
  viz::SurfaceId surface_id(viz::FrameSinkId(2u, 3u),
                            viz::LocalSurfaceId(4u, 5u, token));
  cc::DeadlinePolicy deadline_policy = cc::DeadlinePolicy::UseDefaultDeadline();
  child_surface_layer->SetSurfaceId(surface_id, deadline_policy);

  {
    std::optional<viz::HitTestRegionList> hit_test_region_list;
    viz::CompositorFrame frame = ProduceFrame(&hit_test_region_list);

    ASSERT_TRUE(hit_test_region_list);
    EXPECT_EQ(hit_test_region_list->bounds, viewport_);

    ASSERT_EQ(hit_test_region_list->regions.size(), 2u);
    auto& root_region = hit_test_region_list->regions.back();
    EXPECT_EQ(root_region.frame_sink_id, viz::FrameSinkId(1u, 2u));
    EXPECT_EQ(root_region.rect, viewport_);
    EXPECT_EQ(root_region.transform, gfx::Transform());

    auto& child_region = hit_test_region_list->regions.front();
    EXPECT_EQ(child_region.frame_sink_id, viz::FrameSinkId(2u, 3u));
    EXPECT_EQ(child_region.rect, gfx::Rect(10, 10));

    gfx::Transform expected_transform =
        gfx::Transform::MakeTranslation(5.0f, 5.0f);
    expected_transform.Rotate(-45.0);
    expected_transform.Translate(-5.0f, -5.0f);
    expected_transform.Translate(-10.0f, -10.0f);

    EXPECT_TRANSFORM_NEAR(child_region.transform, expected_transform, 1e-15);
    EXPECT_TRUE(child_region.flags | viz::HitTestRegionFlags::kHitTestAsk);
    EXPECT_TRUE(child_region.async_hit_test_reasons |
                viz::AsyncHitTestReasons::kIrregularClip);
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, HitTestRegionInNonRootPass) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto filter_layer = Layer::Create();
  filter_layer->SetBounds(gfx::Size(50, 50));
  filter_layer->SetPosition(gfx::PointF(10.0f, 10.0f));
  // Add a filter to force non-root render pass.
  filter_layer->SetFilters({cc::slim::Filter::CreateBrightness(0.5f)});

  auto surface_layer = SurfaceLayer::Create();
  surface_layer->SetBounds(gfx::Size(100, 100));
  surface_layer->SetIsDrawable(true);
  surface_layer->SetTransform(gfx::Transform::MakeScale(0.5));

  base::UnguessableToken token = base::UnguessableToken::Create();
  viz::SurfaceId surface_id(viz::FrameSinkId(1u, 2u),
                            viz::LocalSurfaceId(3u, 4u, token));
  cc::DeadlinePolicy deadline_policy = cc::DeadlinePolicy::UseDefaultDeadline();
  surface_layer->SetSurfaceId(surface_id, deadline_policy);

  root_layer->AddChild(filter_layer);
  filter_layer->AddChild(surface_layer);

  {
    std::optional<viz::HitTestRegionList> hit_test_region_list;
    viz::CompositorFrame frame = ProduceFrame(&hit_test_region_list);
    ASSERT_TRUE(hit_test_region_list);
    EXPECT_EQ(hit_test_region_list->bounds, viewport_);

    ASSERT_EQ(hit_test_region_list->regions.size(), 1u);
    auto& hit_test_region = hit_test_region_list->regions.front();
    EXPECT_EQ(hit_test_region.frame_sink_id, viz::FrameSinkId(1u, 2u));
    EXPECT_EQ(hit_test_region.rect, gfx::Rect(100, 100));
    EXPECT_EQ(hit_test_region.transform,
              gfx::Transform::MakeScale(2.0f) *
                  gfx::Transform::MakeTranslation(-10.0f, -10.0f));
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, NonInvertibleTransform) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto child_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kRed);
  child_layer->SetTransform(gfx::Transform::MakeScale(0.0f));
  root_layer->AddChild(child_layer);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  // Check only child layer does not generate a quad.
  ASSERT_THAT(
      pass->quad_list,
      ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                        viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                        viz::HasTransform(gfx::Transform()))));
}

TEST_F(SlimLayerTreeCompositorFrameTest, VisibleRect) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto clip_and_scale_layer = Layer::Create();
  clip_and_scale_layer->SetMasksToBounds(true);
  // Odd size so halving scaling it by 0.5 results in non-integer rect.
  clip_and_scale_layer->SetBounds(gfx::Size(49, 49));
  clip_and_scale_layer->SetTransform(gfx::Transform::MakeScale(0.5f));
  root_layer->AddChild(clip_and_scale_layer);

  auto clipped_layer = CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  clip_and_scale_layer->AddChild(clipped_layer);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  ASSERT_THAT(
      pass->quad_list,
      ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                        viz::HasRect(gfx::Rect(50, 50)),
                        viz::HasVisibleRect(gfx::Rect(49, 49)),
                        viz::HasTransform(gfx::Transform::MakeScale(0.5f))),
                  AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                        viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                        viz::HasTransform(gfx::Transform()))));
  auto* child_quad = pass->quad_list.front();
  EXPECT_EQ(child_quad->shared_quad_state->quad_layer_rect, gfx::Rect(50, 50));
  EXPECT_EQ(child_quad->shared_quad_state->visible_quad_layer_rect,
            gfx::Rect(49, 49));
  ASSERT_TRUE(child_quad->shared_quad_state->clip_rect);
  // `clip_rect` in target pass space should be rounded up.
  EXPECT_EQ(child_quad->shared_quad_state->clip_rect, gfx::Rect(25, 25));
}

TEST_F(SlimLayerTreeCompositorFrameTest, CompletelyClippedLayer) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto clip_and_scale_layer = Layer::Create();
  clip_and_scale_layer->SetMasksToBounds(true);
  clip_and_scale_layer->SetBounds(gfx::Size(50, 50));
  root_layer->AddChild(clip_and_scale_layer);

  auto clipped_layer = CreateSolidColorLayer(gfx::Size(25, 25), SkColors::kRed);
  clipped_layer->SetPosition(gfx::PointF(60.0f, 60.0f));
  clip_and_scale_layer->AddChild(clipped_layer);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  ASSERT_THAT(
      pass->quad_list,
      ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                        viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                        viz::HasTransform(gfx::Transform()))));
}

TEST_F(SlimLayerTreeCompositorFrameTest, NonAxisAlignedClip) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  // Clip is 50x50 rotated by 45 degrees about the center.
  auto clip_layer = cc::slim::Layer::Create();
  clip_layer->SetMasksToBounds(true);
  clip_layer->SetBounds(gfx::Size(50, 50));
  clip_layer->SetTransformOrigin(gfx::PointF(25.0f, 25.0f));
  gfx::Transform transform;
  transform.Rotate(45);
  clip_layer->SetTransform(transform);
  root_layer->AddChild(clip_layer);

  // Drawing layer is 80x80 larger than the clip.
  auto solid_color_layer =
      CreateSolidColorLayer(gfx::Size(80, 80), SkColors::kRed);
  clip_layer->AddChild(std::move(solid_color_layer));

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 2u);
  auto& child_pass = frame.render_pass_list.front();
  EXPECT_EQ(child_pass->output_rect, gfx::Rect(50, 50));
  EXPECT_EQ(child_pass->damage_rect, gfx::Rect(50, 50));
  gfx::Transform child_pass_transform =
      gfx::Transform::MakeTranslation(25.0f, 25.0f);
  child_pass_transform.PreConcat(transform);
  child_pass_transform.PreConcat(
      gfx::Transform::MakeTranslation(-25.0f, -25.0f));
  EXPECT_EQ(child_pass->transform_to_root_target, child_pass_transform);
  EXPECT_THAT(child_pass->quad_list,
              ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                viz::HasRect(gfx::Rect(80, 80)),
                                viz::HasVisibleRect(gfx::Rect(50, 50)),
                                viz::HasTransform(gfx::Transform()))));

  auto& root_pass = frame.render_pass_list.back();
  ASSERT_THAT(
      root_pass->quad_list,
      ElementsAre(AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                        viz::HasRect(gfx::Rect(50, 50)),
                        viz::HasVisibleRect(gfx::Rect(50, 50)),
                        viz::HasTransform(child_pass_transform)),
                  AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                        viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                        viz::HasTransform(gfx::Transform()))));
  auto* render_pass_quad = viz::CompositorRenderPassDrawQuad::MaterialCast(
      root_pass->quad_list.ElementAt(0));
  auto* shared_quad_state = render_pass_quad->shared_quad_state;
  EXPECT_EQ(shared_quad_state->quad_layer_rect, gfx::Rect(50, 50));
  EXPECT_EQ(shared_quad_state->visible_quad_layer_rect, gfx::Rect(50, 50));
  EXPECT_EQ(shared_quad_state->clip_rect, std::nullopt);
}

TEST_F(SlimLayerTreeCompositorFrameTest, ChildPassOutputRect) {
  // Tests that child render pass is only sized to areas with content.
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  // Clip is 50x50 rotated by 45 degrees about the center.
  auto clip_layer = cc::slim::Layer::Create();
  clip_layer->SetMasksToBounds(true);
  clip_layer->SetBounds(gfx::Size(50, 50));
  clip_layer->SetTransformOrigin(gfx::PointF(25.0f, 25.0f));
  gfx::Transform transform;
  transform.Rotate(45);
  clip_layer->SetTransform(transform);
  root_layer->AddChild(clip_layer);

  // Drawing layer is 80x80 offset by 20,20.
  auto solid_color_layer =
      CreateSolidColorLayer(gfx::Size(80, 80), SkColors::kRed);
  solid_color_layer->SetPosition(gfx::PointF(20.0f, 20.0f));
  clip_layer->AddChild(std::move(solid_color_layer));

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 2u);
  auto& child_pass = frame.render_pass_list.front();
  // Child pass should only have size 30x30.
  EXPECT_EQ(child_pass->output_rect, gfx::Rect(20, 20, 30, 30));
  EXPECT_EQ(child_pass->damage_rect, gfx::Rect(20, 20, 30, 30));
  gfx::Transform child_pass_transform =
      gfx::Transform::MakeTranslation(25.0f, 25.0f);
  child_pass_transform.PreConcat(transform);
  child_pass_transform.PreConcat(
      gfx::Transform::MakeTranslation(-25.0f, -25.0f));
  EXPECT_EQ(child_pass->transform_to_root_target, child_pass_transform);
  ASSERT_THAT(
      child_pass->quad_list,
      ElementsAre(AllOf(
          viz::IsSolidColorQuad(SkColors::kRed),
          viz::HasRect(gfx::Rect(80, 80)),
          // Visible rect is clipped.
          viz::HasVisibleRect(gfx::Rect(30, 30)),
          viz::HasTransform(gfx::Transform::MakeTranslation(20.0f, 20.0f)))));
  {
    // SharedQuadState should match the quad.
    auto* shared_quad_state =
        child_pass->quad_list.ElementAt(0)->shared_quad_state;
    EXPECT_EQ(shared_quad_state->quad_layer_rect, gfx::Rect(80, 80));
    EXPECT_EQ(shared_quad_state->visible_quad_layer_rect, gfx::Rect(30, 30));
  }

  auto& root_pass = frame.render_pass_list.back();
  ASSERT_THAT(
      root_pass->quad_list,
      ElementsAre(AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                        viz::HasRect(gfx::Rect(20, 20, 30, 30)),
                        viz::HasVisibleRect(gfx::Rect(20, 20, 30, 30)),
                        viz::HasTransform(child_pass_transform)),
                  AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                        viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                        viz::HasTransform(gfx::Transform()))));
  {
    auto* render_pass_quad = viz::CompositorRenderPassDrawQuad::MaterialCast(
        root_pass->quad_list.ElementAt(0));
    auto* shared_quad_state = render_pass_quad->shared_quad_state;
    EXPECT_EQ(shared_quad_state->quad_layer_rect, gfx::Rect(20, 20, 30, 30));
    EXPECT_EQ(shared_quad_state->visible_quad_layer_rect,
              gfx::Rect(20, 20, 30, 30));
    EXPECT_EQ(shared_quad_state->clip_rect, std::nullopt);
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, Filters) {
  // Also tests that scale down applies to child pass.
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  // Child layer has filters so require a child render pass.
  // Child is scaled down and translated.
  auto solid_color_layer =
      CreateSolidColorLayer(gfx::Size(80, 80), SkColors::kRed);
  solid_color_layer->SetTransform(gfx::Transform::MakeScale(0.5f));
  solid_color_layer->SetFilters({cc::slim::Filter::CreateBrightness(0.5f)});
  solid_color_layer->SetPosition(gfx::PointF(10.0f, 10.0f));
  root_layer->AddChild(std::move(solid_color_layer));

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 2u);
  auto& child_pass = frame.render_pass_list.front();
  // Child pass should be scaled down.
  EXPECT_EQ(child_pass->output_rect, gfx::Rect(40, 40));
  EXPECT_EQ(child_pass->damage_rect, gfx::Rect(40, 40));
  // Pass is translated.
  gfx::Transform child_pass_transform =
      gfx::Transform::MakeTranslation(10.0f, 10.0f);
  EXPECT_EQ(child_pass->transform_to_root_target, child_pass_transform);
  ASSERT_THAT(child_pass->quad_list,
              ElementsAre(AllOf(
                  viz::IsSolidColorQuad(SkColors::kRed),
                  viz::HasRect(gfx::Rect(80, 80)),
                  // Visible rect is clipped.
                  viz::HasVisibleRect(gfx::Rect(80, 80)),
                  // Scaled down to fit the child pass.
                  viz::HasTransform(gfx::Transform::MakeScale(0.5f, 0.5f)))));
  {
    // SharedQuadState should match the quad.
    auto* shared_quad_state =
        child_pass->quad_list.ElementAt(0)->shared_quad_state;
    EXPECT_EQ(shared_quad_state->quad_layer_rect, gfx::Rect(80, 80));
    EXPECT_EQ(shared_quad_state->visible_quad_layer_rect, gfx::Rect(80, 80));
  }
  EXPECT_THAT(child_pass->filters.operations(),
              ElementsAre(cc::FilterOperation::CreateBrightnessFilter(0.5f)));

  auto& root_pass = frame.render_pass_list.back();
  ASSERT_THAT(
      root_pass->quad_list,
      ElementsAre(AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                        // Render pass quad matches pass size.
                        viz::HasRect(gfx::Rect(40, 40)),
                        viz::HasVisibleRect(gfx::Rect(40, 40)),
                        // Quad is only translated.
                        viz::HasTransform(
                            gfx::Transform::MakeTranslation(10.0f, 10.0f))),
                  AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                        viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                        viz::HasTransform(gfx::Transform()))));
  {
    auto* render_pass_quad = viz::CompositorRenderPassDrawQuad::MaterialCast(
        root_pass->quad_list.ElementAt(0));
    auto* shared_quad_state = render_pass_quad->shared_quad_state;
    EXPECT_EQ(shared_quad_state->quad_layer_rect, gfx::Rect(40, 40));
    EXPECT_EQ(shared_quad_state->visible_quad_layer_rect, gfx::Rect(40, 40));
    EXPECT_EQ(shared_quad_state->clip_rect, std::nullopt);
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, FiltersOnNonDrawingLayer) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto filter_layer = cc::slim::Layer::Create();
  filter_layer->SetFilters({cc::slim::Filter::CreateBrightness(0.5f)});
  auto solid_color_layer =
      CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  filter_layer->AddChild(solid_color_layer);
  root_layer->AddChild(filter_layer);

  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    ASSERT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                  viz::HasRect(gfx::Rect(50, 50)),
                                  viz::HasVisibleRect(gfx::Rect(50, 50)))));
    EXPECT_EQ(child_pass->output_rect, gfx::Rect(50, 50));
    EXPECT_THAT(child_pass->filters.operations(),
                ElementsAre(cc::FilterOperation::CreateBrightnessFilter(0.5f)));
    auto& root_pass = frame.render_pass_list.back();
    ASSERT_THAT(
        root_pass->quad_list,
        ElementsAre(AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                          viz::HasRect(gfx::Rect(50, 50)),
                          viz::HasVisibleRect(gfx::Rect(50, 50))),
                    viz::IsSolidColorQuad(SkColors::kGray)));
  }

  // Clip the child pass.
  filter_layer->SetBounds(gfx::Size(25, 25));
  filter_layer->SetMasksToBounds(true);
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    ASSERT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                  viz::HasRect(gfx::Rect(50, 50)),
                                  viz::HasVisibleRect(gfx::Rect(25, 25)))));
    EXPECT_EQ(child_pass->output_rect, gfx::Rect(25, 25));
    EXPECT_THAT(child_pass->filters.operations(),
                ElementsAre(cc::FilterOperation::CreateBrightnessFilter(0.5f)));
    auto& root_pass = frame.render_pass_list.back();
    ASSERT_THAT(
        root_pass->quad_list,
        ElementsAre(AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                          viz::HasRect(gfx::Rect(25, 25)),
                          viz::HasVisibleRect(gfx::Rect(25, 25))),
                    viz::IsSolidColorQuad(SkColors::kGray)));
  }

  // Completely clip the child pass.
  filter_layer->SetBounds(gfx::Size(0, 0));
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& root_pass = frame.render_pass_list.back();
    ASSERT_THAT(root_pass->quad_list,
                ElementsAre(viz::IsSolidColorQuad(SkColors::kGray)));
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, Opacity) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  // Child will require a render pass to blend correctly.
  auto child_layer = CreateSolidColorLayer(gfx::Size(80, 80), SkColors::kRed);
  child_layer->SetOpacity(0.75f);

  // Grand child does not need another render pass because it does not have
  // 2 drawing layers.
  auto grand_child_layer =
      CreateSolidColorLayer(gfx::Size(40, 40), SkColors::kGreen);
  grand_child_layer->SetOpacity(0.5f);

  child_layer->AddChild(std::move(grand_child_layer));
  root_layer->AddChild(std::move(child_layer));

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 2u);

  auto& child_pass = frame.render_pass_list.front();
  EXPECT_EQ(child_pass->output_rect, gfx::Rect(80, 80));
  EXPECT_EQ(child_pass->damage_rect, gfx::Rect(80, 80));
  EXPECT_EQ(child_pass->transform_to_root_target, gfx::Transform());
  ASSERT_THAT(
      child_pass->quad_list,
      ElementsAre(
          AllOf(viz::IsSolidColorQuad(SkColors::kGreen),
                viz::HasRect(gfx::Rect(40, 40)),
                viz::HasVisibleRect(gfx::Rect(40, 40)),
                viz::HasTransform(gfx::Transform()),
                // The pass is drawn with the child_layer's opacity, so there is
                // no multiplicative opacity here.
                viz::HasOpacity(0.5f)),
          AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                viz::HasRect(gfx::Rect(80, 80)),
                viz::HasVisibleRect(gfx::Rect(80, 80)),
                viz::HasTransform(gfx::Transform()), viz::HasOpacity(1.0f))));

  auto& root_pass = frame.render_pass_list.back();
  ASSERT_THAT(
      root_pass->quad_list,
      ElementsAre(
          AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                viz::HasRect(gfx::Rect(80, 80)),
                viz::HasVisibleRect(gfx::Rect(80, 80)),
                viz::HasTransform(gfx::Transform()), viz::HasOpacity(0.75f)),
          AllOf(viz::IsSolidColorQuad(SkColors::kGray), viz::HasRect(viewport_),
                viz::HasVisibleRect(viewport_),
                viz::HasTransform(gfx::Transform()))));
}

TEST_F(SlimLayerTreeCompositorFrameTest, SkipZeroOpacitySubtree) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto child_layer = CreateSolidColorLayer(gfx::Size(80, 80), SkColors::kRed);
  child_layer->SetOpacity(0.0f);
  auto grand_child_layer =
      CreateSolidColorLayer(gfx::Size(40, 40), SkColors::kGreen);
  child_layer->AddChild(std::move(grand_child_layer));
  root_layer->AddChild(std::move(child_layer));

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);

  auto& root_pass = frame.render_pass_list.back();
  EXPECT_THAT(
      root_pass->quad_list,
      ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                        viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                        viz::HasTransform(gfx::Transform()))));
}

TEST_F(SlimLayerTreeCompositorFrameTest, SimpleOcclusion) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto partially_occluded_layer =
      CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  partially_occluded_layer->SetPosition(gfx::PointF(25.0f, 25.0f));

  // Occlude top 10 pixels.
  auto sibling_occlusion_layer =
      CreateSolidColorLayer(gfx::Size(50, 10), SkColors::kGreen);
  // Position relative to root.
  sibling_occlusion_layer->SetPosition(gfx::PointF(25.0f, 25.0f));

  // Occlude the next top 10 pixels.
  auto child_occlusion_layer =
      CreateSolidColorLayer(gfx::Size(50, 10), SkColors::kBlue);
  // Position relative to `partially_occluded_layer`.
  child_occlusion_layer->SetPosition(gfx::PointF(0.0f, 10.0f));

  partially_occluded_layer->AddChild(std::move(child_occlusion_layer));
  root_layer->AddChild(std::move(partially_occluded_layer));
  root_layer->AddChild(std::move(sibling_occlusion_layer));

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  ASSERT_THAT(pass->quad_list,
              ElementsAre(viz::IsSolidColorQuad(SkColors::kGreen),
                          viz::IsSolidColorQuad(SkColors::kBlue),
                          AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                viz::HasRect(gfx::Rect(50, 50)),
                                viz::HasVisibleRect(gfx::Rect(0, 20, 50, 30))),
                          AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                                viz::HasRect(viewport_),
                                viz::HasVisibleRect(viewport_))));
}

TEST_F(SlimLayerTreeCompositorFrameTest, OcclusionWithNonOpaqueLayer) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto lower_layer = CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  root_layer->AddChild(lower_layer);

  // Middle layer is not opaque so should not contribute to occlusion.
  auto middle_layer =
      CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kGreen);
  middle_layer->SetPosition(gfx::PointF(25.0f, 0.0f));
  middle_layer->SetOpacity(0.5f);
  root_layer->AddChild(middle_layer);

  // Top layer should partially occlude middle layer.
  auto top_layer = CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kBlue);
  top_layer->SetPosition(gfx::PointF(50.0f, 0.0f));
  root_layer->AddChild(top_layer);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  EXPECT_THAT(
      pass->quad_list,
      ElementsAre(
          AllOf(viz::IsSolidColorQuad(SkColors::kBlue),
                viz::HasVisibleRect(gfx::Rect(50, 50))),
          AllOf(viz::IsSolidColorQuad(SkColors::kGreen),
                // Middle layer occluded on the right by top layer.
                viz::HasVisibleRect(gfx::Rect(25, 50))),
          AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                // Lower layer not occluded by non-opaque middle layer.
                viz::HasVisibleRect(gfx::Rect(50, 50))),
          AllOf(viz::IsSolidColorQuad(SkColors::kGray), viz::HasRect(viewport_),
                // Top half is occluded by lower and top layer.
                viz::HasVisibleRect(gfx::Rect(0, 50, 100, 50)))));
}

TEST_F(SlimLayerTreeCompositorFrameTest, OcclusionWithRenderPass) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto child_pass_root = Layer::Create();
  child_pass_root->SetFilters({cc::slim::Filter::CreateBrightness(0.5f)});
  // Set size and scale to half of viewport.
  child_pass_root->SetBounds(gfx::Size(200, 100));
  child_pass_root->SetTransform(gfx::Transform::MakeScale(0.5f));
  child_pass_root->SetPosition(gfx::PointF(0.0f, 50.0f));
  root_layer->AddChild(child_pass_root);

  auto child_pass_layer =
      CreateSolidColorLayer(gfx::Size(200, 100), SkColors::kRed);
  child_pass_root->AddChild(child_pass_layer);

  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    ASSERT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                  viz::HasRect(gfx::Rect(200, 100)),
                                  viz::HasVisibleRect(gfx::Rect(200, 100)))));

    auto& root_pass = frame.render_pass_list.back();
    ASSERT_THAT(
        root_pass->quad_list,
        ElementsAre(AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                          // RenderPassQuad is fully covered by quads.
                          viz::AreContentsOpaque(true)),
                    AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                          viz::HasRect(viewport_),
                          // Occluded by child pass.
                          viz::HasVisibleRect(gfx::Rect(100, 50)))));
  }

  // Move child pass to the top and move layer in pass to top half of pass.
  child_pass_root->SetPosition(gfx::PointF(0.0f, 0.0f));
  child_pass_layer->SetBounds(gfx::Size(200, 50));
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    ASSERT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                  viz::HasRect(gfx::Rect(200, 50)),
                                  // Only top half is covered.
                                  viz::HasVisibleRect(gfx::Rect(200, 50)))));

    auto& root_pass = frame.render_pass_list.back();
    ASSERT_THAT(
        root_pass->quad_list,
        ElementsAre(AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                          // Pass rects shrinks to content rect.
                          viz::HasRect(gfx::Rect(100, 25)),
                          viz::HasVisibleRect(gfx::Rect(100, 25)),
                          viz::AreContentsOpaque(true)),
                    AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                          viz::HasRect(viewport_),
                          // Occluded by child pass.
                          viz::HasVisibleRect(gfx::Rect(0, 25, 100, 75)))));
  }

  // Add another layer so that the bottom right corner of the render pass is not
  // covered.
  auto child_pass_layer_2 =
      CreateSolidColorLayer(gfx::Size(100, 50), SkColors::kGreen);
  child_pass_layer_2->SetPosition(gfx::PointF(0.0f, 50.0f));
  child_pass_root->AddChild(child_pass_layer_2);
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    ASSERT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kGreen),
                                  viz::HasRect(gfx::Rect(100, 50)),
                                  viz::HasVisibleRect(gfx::Rect(100, 50))),
                            AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                  viz::HasRect(gfx::Rect(200, 50)),
                                  viz::HasVisibleRect(gfx::Rect(200, 50)))));

    auto& root_pass = frame.render_pass_list.back();
    ASSERT_THAT(
        root_pass->quad_list,
        ElementsAre(AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                          viz::HasRect(gfx::Rect(100, 50)),
                          viz::HasVisibleRect(gfx::Rect(100, 50)),
                          // RenderPassQuad is fully covered.
                          viz::AreContentsOpaque(false)),
                    AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                          viz::HasRect(viewport_),
                          // Occluded by child pass.
                          viz::HasVisibleRect(gfx::Rect(0, 25, 100, 75)))));
  }

  // Add another layer to fully cover the child pass.
  auto child_pass_layer_3 =
      CreateSolidColorLayer(gfx::Size(100, 50), SkColors::kBlue);
  child_pass_layer_3->SetPosition(gfx::PointF(100.0f, 50.0f));
  child_pass_root->AddChild(child_pass_layer_3);
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    ASSERT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kBlue),
                                  viz::HasRect(gfx::Rect(100, 50)),
                                  viz::HasVisibleRect(gfx::Rect(100, 50))),
                            AllOf(viz::IsSolidColorQuad(SkColors::kGreen),
                                  viz::HasRect(gfx::Rect(100, 50)),
                                  viz::HasVisibleRect(gfx::Rect(100, 50))),
                            AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                  viz::HasRect(gfx::Rect(200, 50)),
                                  viz::HasVisibleRect(gfx::Rect(200, 50)))));

    auto& root_pass = frame.render_pass_list.back();
    ASSERT_THAT(
        root_pass->quad_list,
        ElementsAre(AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                          viz::HasRect(gfx::Rect(100, 50)),
                          viz::HasVisibleRect(gfx::Rect(100, 50)),
                          // RenderPassQuad is fully covered.
                          viz::AreContentsOpaque(true)),
                    AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                          viz::HasRect(viewport_),
                          // Occluded by child pass.
                          viz::HasVisibleRect(gfx::Rect(0, 50, 100, 50)))));
  }

  // Expand child layer so it's partially occluded by child layer 2 and 3.
  child_pass_layer->SetBounds(gfx::Size(200, 100));
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    ASSERT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kBlue),
                                  viz::HasRect(gfx::Rect(100, 50)),
                                  viz::HasVisibleRect(gfx::Rect(100, 50))),
                            AllOf(viz::IsSolidColorQuad(SkColors::kGreen),
                                  viz::HasRect(gfx::Rect(100, 50)),
                                  viz::HasVisibleRect(gfx::Rect(100, 50))),
                            AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                  viz::HasRect(gfx::Rect(200, 100)),
                                  // Partially occluded.
                                  viz::HasVisibleRect(gfx::Rect(200, 50)))));

    auto& root_pass = frame.render_pass_list.back();
    ASSERT_THAT(
        root_pass->quad_list,
        ElementsAre(AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                          // RenderPassQuad is fully covered.
                          viz::AreContentsOpaque(true)),
                    AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                          viz::HasRect(viewport_),
                          // Occluded by child pass.
                          viz::HasVisibleRect(gfx::Rect(0, 50, 100, 50)))));
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, OccludedNonOpaqueBackgroundColor) {
  // Check that even if background color is not opaque, the frame should still
  // be opaque if the viewport is entirely occluded by opaque layers.
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->set_background_color(SkColors::kTransparent);
  layer_tree_->SetRoot(root_layer);
  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  ASSERT_THAT(pass->quad_list,
              ElementsAre(viz::IsSolidColorQuad(SkColors::kGray)));
  EXPECT_FALSE(pass->has_transparent_background);
}

TEST_F(SlimLayerTreeCompositorFrameTest, Guttering) {
  auto root_layer = CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  root_layer->SetPosition(gfx::PointF(25.0f, 25.0f));
  layer_tree_->SetRoot(root_layer);
  layer_tree_->set_background_color(SkColors::kBlue);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.front();
  EXPECT_THAT(pass->quad_list,
              ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                viz::HasRect(gfx::Rect(50, 50)),
                                viz::HasVisibleRect(gfx::Rect(50, 50))),
                          // Should require 4 gutter quads.
                          viz::IsSolidColorQuad(SkColors::kBlue),
                          viz::IsSolidColorQuad(SkColors::kBlue),
                          viz::IsSolidColorQuad(SkColors::kBlue),
                          viz::IsSolidColorQuad(SkColors::kBlue)));
  EXPECT_FALSE(pass->has_transparent_background);

  Region region;
  for (auto& quad : frame.render_pass_list) {
    region.Union(quad->output_rect);
  }
  EXPECT_TRUE(region.Contains(viewport_));
}

TEST_F(SlimLayerTreeCompositorFrameTest, PropertyDamage) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto solid_color_layer =
      CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  root_layer->AddChild(solid_color_layer);

  auto check_frame = [&](SkColor4f color, gfx::Rect damage) {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    EXPECT_THAT(pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(color),
                                  viz::HasRect(gfx::Rect(50, 50))),
                            AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                                  viz::HasRect(viewport_),
                                  viz::HasVisibleRect(viewport_),
                                  viz::HasTransform(gfx::Transform()))));
    EXPECT_EQ(pass->damage_rect, damage);
  };

  // First frame should have full damage.
  check_frame(SkColors::kRed, viewport_);

  solid_color_layer->SetBackgroundColor(SkColors::kGreen);
  // Damage only the layer.
  check_frame(SkColors::kGreen, gfx::Rect(50, 50));

  solid_color_layer->SetPosition(gfx::PointF(10.2f, 10.2f));
  // Damage newly exposed area as well. Also damage is rounded to enclosing
  // rect.
  check_frame(SkColors::kGreen, gfx::Rect(61, 61));

  // Damage is empty if there is no change.
  check_frame(SkColors::kGreen, gfx::Rect());
}

TEST_F(SlimLayerTreeCompositorFrameTest, PropertyChangeFromParentDamage) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto parent = Layer::Create();
  auto solid_color_layer =
      CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  parent->AddChild(solid_color_layer);
  root_layer->AddChild(parent);

  auto check_frame = [&](gfx::Rect damage) {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    EXPECT_THAT(pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                  viz::HasRect(gfx::Rect(50, 50))),
                            AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                                  viz::HasRect(viewport_),
                                  viz::HasVisibleRect(viewport_),
                                  viz::HasTransform(gfx::Transform()))));
    EXPECT_EQ(pass->damage_rect, damage);
  };

  // First frame should have full damage.
  check_frame(viewport_);

  parent->SetPosition(gfx::PointF(10.0f, 10.0f));
  // Damage newly exposed area as well.
  check_frame(gfx::Rect(60, 60));

  parent->SetOpacity(0.5f);
  check_frame(gfx::Rect(10, 10, 50, 50));

  // Rotate about center, which does not change visible rect.
  parent->SetTransformOrigin(gfx::PointF(25.0f, 25.0f));
  parent->SetTransform(gfx::Transform::Make90degRotation());
  check_frame(gfx::Rect(10, 10, 50, 50));

  // Damage is empty if there is no change.
  check_frame(gfx::Rect());

  solid_color_layer->RemoveFromParent();
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    EXPECT_THAT(pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                                  viz::HasRect(viewport_),
                                  viz::HasVisibleRect(viewport_),
                                  viz::HasTransform(gfx::Transform()))));
    // Removed layer damages exposed area.
    EXPECT_EQ(pass->damage_rect, gfx::Rect(10, 10, 50, 50));
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, NonRootPassDamage) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto parent = Layer::Create();
  auto solid_color_layer =
      CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  parent->AddChild(solid_color_layer);
  root_layer->AddChild(parent);

  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    EXPECT_THAT(pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                  viz::HasRect(gfx::Rect(50, 50))),
                            AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                                  viz::HasRect(viewport_),
                                  viz::HasVisibleRect(viewport_),
                                  viz::HasTransform(gfx::Transform()))));
    // First frame should have full damage.
    EXPECT_EQ(pass->damage_rect, viewport_);
  }

  parent->SetFilters({cc::slim::Filter::CreateBrightness(0.5f)});
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    EXPECT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                  viz::HasRect(gfx::Rect(50, 50)),
                                  viz::HasVisibleRect(gfx::Rect(50, 50)))));
    EXPECT_EQ(child_pass->damage_rect, gfx::Rect(50, 50));

    auto& root_pass = frame.render_pass_list.back();
    EXPECT_THAT(
        root_pass->quad_list,
        ElementsAre(
            AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                  viz::HasRect(gfx::Rect(50, 50))),
            AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                  viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                  viz::HasTransform(gfx::Transform()))));
    EXPECT_EQ(root_pass->damage_rect, gfx::Rect(50, 50));
  }

  // new frame with no change should not have damage.
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    EXPECT_EQ(child_pass->damage_rect, gfx::Rect());
    auto& root_pass = frame.render_pass_list.back();
    EXPECT_EQ(root_pass->damage_rect, gfx::Rect());
  }

  // Changing child layer damages both passes.
  solid_color_layer->SetBackgroundColor(SkColors::kBlue);
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    EXPECT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kBlue),
                                  viz::HasRect(gfx::Rect(50, 50)),
                                  viz::HasVisibleRect(gfx::Rect(50, 50)))));
    EXPECT_EQ(child_pass->damage_rect, gfx::Rect(50, 50));

    auto& root_pass = frame.render_pass_list.back();
    EXPECT_THAT(
        root_pass->quad_list,
        ElementsAre(
            AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                  viz::HasRect(gfx::Rect(50, 50))),
            AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                  viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                  viz::HasTransform(gfx::Transform()))));
    EXPECT_EQ(root_pass->damage_rect, gfx::Rect(50, 50));
  }

  // Moving child pass damages root pass.
  parent->SetPosition(gfx::PointF(25.0f, 25.0f));
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    EXPECT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kBlue),
                                  viz::HasRect(gfx::Rect(50, 50)),
                                  viz::HasVisibleRect(gfx::Rect(50, 50)))));
    // Child pass damage rect ideally can be empty here because none of the
    // layers inside the pass changed in relation to the pass. Current
    // implementation uses Layer::NotifySubtreeChanged that damages the whole
    // subtree across render passes which is why the child pass is damaged.
    // Getting damage correct may be tricky and brittle, and currently viz
    // ignores damage on non-root render passes, so this case is not
    // implemented.
    EXPECT_EQ(child_pass->damage_rect, gfx::Rect(50, 50));

    auto& root_pass = frame.render_pass_list.back();
    EXPECT_THAT(
        root_pass->quad_list,
        ElementsAre(
            AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                  viz::HasRect(gfx::Rect(50, 50))),
            AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                  viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                  viz::HasTransform(gfx::Transform()))));
    EXPECT_EQ(root_pass->damage_rect, gfx::Rect(75, 75));
  }

  // Adding a layer outside the child pass and check child pass is not damaged.
  root_layer->AddChild(
      CreateSolidColorLayer(gfx::Size(10, 10), SkColors::kGreen));
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    auto& child_pass = frame.render_pass_list.front();
    EXPECT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kBlue),
                                  viz::HasRect(gfx::Rect(50, 50)),
                                  viz::HasVisibleRect(gfx::Rect(50, 50)))));
    EXPECT_EQ(child_pass->damage_rect, gfx::Rect());

    auto& root_pass = frame.render_pass_list.back();
    EXPECT_THAT(
        root_pass->quad_list,
        ElementsAre(
            AllOf(viz::IsSolidColorQuad(SkColors::kGreen),
                  viz::HasRect(gfx::Rect(10, 10)),
                  viz::HasVisibleRect(gfx::Rect(10, 10))),
            AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                  viz::HasRect(gfx::Rect(50, 50))),
            AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                  viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                  viz::HasTransform(gfx::Transform()))));
    EXPECT_EQ(root_pass->damage_rect, gfx::Rect(10, 10));
  }

  // Removing child pass damages parent pass.
  solid_color_layer->RemoveFromParent();
  {
    viz::CompositorFrame frame = ProduceFrame();
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& pass = frame.render_pass_list.back();
    EXPECT_THAT(pass->quad_list,
                ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kGreen),
                                  viz::HasRect(gfx::Rect(10, 10)),
                                  viz::HasVisibleRect(gfx::Rect(10, 10))),
                            AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                                  viz::HasRect(viewport_),
                                  viz::HasVisibleRect(viewport_),
                                  viz::HasTransform(gfx::Transform()))));
    EXPECT_EQ(pass->damage_rect, gfx::Rect(25, 25, 50, 50));
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, SimpleRoundedCorner) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto solid_color_layer =
      CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  solid_color_layer->SetRoundedCorner(gfx::RoundedCornersF(20.0f));
  solid_color_layer->SetPosition(gfx::PointF(10.0f, 10.0f));
  root_layer->AddChild(solid_color_layer);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  ASSERT_THAT(
      pass->quad_list,
      ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                        viz::HasRect(gfx::Rect(50, 50)),
                        viz::HasTransform(
                            gfx::Transform::MakeTranslation(10.0f, 10.0f))),
                  AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                        viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                        viz::HasTransform(gfx::Transform()))));
  auto* quad = pass->quad_list.front();
  auto* shared_quad_state = quad->shared_quad_state;
  EXPECT_TRUE(shared_quad_state->mask_filter_info.HasRoundedCorners());
  EXPECT_TRUE(shared_quad_state->is_fast_rounded_corner);
  EXPECT_EQ(shared_quad_state->mask_filter_info.rounded_corner_bounds(),
            gfx::RRectF(10.0f, 10.0f, 50.0f, 50.0f, 20.0f));
}

TEST_F(SlimLayerTreeCompositorFrameTest, RoundedCornerWithChild) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto rounded_corner_layer =
      CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  rounded_corner_layer->SetRoundedCorner(gfx::RoundedCornersF(20.0f));
  rounded_corner_layer->SetPosition(gfx::PointF(10.0f, 10.0f));
  root_layer->AddChild(rounded_corner_layer);

  auto child = CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kBlue);
  child->SetPosition(gfx::PointF(10.0f, 10.0f));
  rounded_corner_layer->AddChild(child);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  ASSERT_THAT(
      pass->quad_list,
      ElementsAre(
          AllOf(
              viz::IsSolidColorQuad(SkColors::kBlue),
              viz::HasRect(gfx::Rect(50, 50)),
              viz::HasVisibleRect(gfx::Rect(40, 40)),
              viz::HasTransform(gfx::Transform::MakeTranslation(20.0f, 20.0f))),
          AllOf(
              viz::IsSolidColorQuad(SkColors::kRed),
              viz::HasRect(gfx::Rect(50, 50)),
              viz::HasTransform(gfx::Transform::MakeTranslation(10.0f, 10.0f))),
          AllOf(viz::IsSolidColorQuad(SkColors::kGray), viz::HasRect(viewport_),
                viz::HasVisibleRect(viewport_),
                viz::HasTransform(gfx::Transform()))));

  const gfx::RRectF expected_rounded_conrer_in_target(10.0f, 10.0f, 50.0f,
                                                      50.0f, 20.0f);
  {
    auto* quad = pass->quad_list.front();
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_TRUE(shared_quad_state->mask_filter_info.HasRoundedCorners());
    EXPECT_TRUE(shared_quad_state->is_fast_rounded_corner);
    EXPECT_EQ(shared_quad_state->mask_filter_info.rounded_corner_bounds(),
              expected_rounded_conrer_in_target);
  }

  {
    auto* quad = pass->quad_list.ElementAt(1u);
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_TRUE(shared_quad_state->mask_filter_info.HasRoundedCorners());
    EXPECT_TRUE(shared_quad_state->is_fast_rounded_corner);
    EXPECT_EQ(shared_quad_state->mask_filter_info.rounded_corner_bounds(),
              expected_rounded_conrer_in_target);
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, NonAxisAlignedRoundedCorner) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto rounded_corner_layer =
      CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  rounded_corner_layer->SetRoundedCorner(gfx::RoundedCornersF(20.0f));
  rounded_corner_layer->SetTransformOrigin(gfx::PointF(25.0f, 25.0f));
  gfx::Transform transform;
  transform.Rotate(45);
  rounded_corner_layer->SetTransform(transform);
  root_layer->AddChild(rounded_corner_layer);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 2u);

  auto& child_pass = frame.render_pass_list.front();
  ASSERT_THAT(child_pass->quad_list,
              ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                viz::HasRect(gfx::Rect(50, 50)),
                                viz::HasVisibleRect(gfx::Rect(50, 50)),
                                viz::HasTransform(gfx::Transform()))));
  {
    auto* quad = child_pass->quad_list.front();
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_TRUE(shared_quad_state->mask_filter_info.HasRoundedCorners());
    EXPECT_TRUE(shared_quad_state->is_fast_rounded_corner);
    EXPECT_EQ(shared_quad_state->mask_filter_info.rounded_corner_bounds(),
              gfx::RRectF(0.0f, 0.0f, 50.0f, 50.0f, 20.0f));
  }

  auto& root_pass = frame.render_pass_list.back();
  gfx::Transform child_pass_transform =
      gfx::Transform::MakeTranslation(25.0f, 25.0f);
  child_pass_transform.PreConcat(transform);
  child_pass_transform.PreConcat(
      gfx::Transform::MakeTranslation(-25.0f, -25.0f));
  ASSERT_THAT(
      root_pass->quad_list,
      ElementsAre(AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                        viz::HasRect(gfx::Rect(50, 50)),
                        viz::HasTransform(child_pass_transform)),
                  AllOf(viz::IsSolidColorQuad(SkColors::kGray),
                        viz::HasRect(viewport_), viz::HasVisibleRect(viewport_),
                        viz::HasTransform(gfx::Transform()))));
  {
    auto* quad = root_pass->quad_list.front();
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_FALSE(shared_quad_state->mask_filter_info.HasRoundedCorners());
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, RoundedCornerOnParentAndChild) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  auto parent = CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  parent->SetRoundedCorner(gfx::RoundedCornersF(20.0f));
  parent->SetPosition(gfx::PointF(10.0f, 10.0f));
  root_layer->AddChild(parent);

  auto child = CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kBlue);
  child->SetPosition(gfx::PointF(10.0f, 10.0f));
  child->SetRoundedCorner(gfx::RoundedCornersF(15.0f));
  parent->AddChild(child);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 2u);

  auto& child_pass = frame.render_pass_list.front();
  ASSERT_THAT(child_pass->quad_list,
              ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kBlue),
                                viz::HasRect(gfx::Rect(50, 50)),
                                viz::HasVisibleRect(gfx::Rect(40, 40)),
                                viz::HasTransform(gfx::Transform()))));
  {
    auto* quad = child_pass->quad_list.front();
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_TRUE(shared_quad_state->mask_filter_info.HasRoundedCorners());
    EXPECT_TRUE(shared_quad_state->is_fast_rounded_corner);
    EXPECT_EQ(shared_quad_state->mask_filter_info.rounded_corner_bounds(),
              gfx::RRectF(0.0f, 0.0f, 50.0f, 50.0f, 15.0f));
  }

  auto& root_pass = frame.render_pass_list.back();
  ASSERT_THAT(
      root_pass->quad_list,
      ElementsAre(
          AllOf(
              viz::IsCompositorRenderPassQuad(child_pass->id),
              viz::HasRect(gfx::Rect(40, 40)),
              viz::HasVisibleRect(gfx::Rect(40, 40)),
              viz::HasTransform(gfx::Transform::MakeTranslation(20.0f, 20.0f))),
          AllOf(
              viz::IsSolidColorQuad(SkColors::kRed),
              viz::HasRect(gfx::Rect(50, 50)),
              viz::HasTransform(gfx::Transform::MakeTranslation(10.0f, 10.0f))),
          AllOf(viz::IsSolidColorQuad(SkColors::kGray), viz::HasRect(viewport_),
                viz::HasVisibleRect(viewport_),
                viz::HasTransform(gfx::Transform()))));

  const gfx::RRectF expected_rounded_conrer_in_target(10.0f, 10.0f, 50.0f,
                                                      50.0f, 20.0f);
  {
    auto* quad = root_pass->quad_list.front();
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_TRUE(shared_quad_state->mask_filter_info.HasRoundedCorners());
    EXPECT_TRUE(shared_quad_state->is_fast_rounded_corner);
    EXPECT_EQ(shared_quad_state->mask_filter_info.rounded_corner_bounds(),
              expected_rounded_conrer_in_target);
  }

  {
    auto* quad = root_pass->quad_list.ElementAt(1u);
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_TRUE(shared_quad_state->mask_filter_info.HasRoundedCorners());
    EXPECT_TRUE(shared_quad_state->is_fast_rounded_corner);
    EXPECT_EQ(shared_quad_state->mask_filter_info.rounded_corner_bounds(),
              expected_rounded_conrer_in_target);
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, GradientMaskWithChild) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  gfx::LinearGradient gradient;
  gradient.AddStep(0.0f, 255);
  gradient.AddStep(1.0f, 0);

  auto gradient_layer =
      CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  gradient_layer->SetGradientMask(gradient);
  gradient_layer->SetPosition(gfx::PointF(10.0f, 10.0f));
  root_layer->AddChild(gradient_layer);

  auto child = CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kBlue);
  child->SetPosition(gfx::PointF(10.0f, 10.0f));
  gradient_layer->AddChild(child);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 1u);
  auto& pass = frame.render_pass_list.back();
  ASSERT_THAT(
      pass->quad_list,
      ElementsAre(
          AllOf(
              viz::IsSolidColorQuad(SkColors::kBlue),
              viz::HasRect(gfx::Rect(50, 50)),
              viz::HasVisibleRect(gfx::Rect(40, 40)),
              viz::HasTransform(gfx::Transform::MakeTranslation(20.0f, 20.0f))),
          AllOf(
              viz::IsSolidColorQuad(SkColors::kRed),
              viz::HasRect(gfx::Rect(50, 50)),
              viz::HasTransform(gfx::Transform::MakeTranslation(10.0f, 10.0f))),
          AllOf(viz::IsSolidColorQuad(SkColors::kGray), viz::HasRect(viewport_),
                viz::HasVisibleRect(viewport_),
                viz::HasTransform(gfx::Transform()))));
  {
    auto* quad = pass->quad_list.front();
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_TRUE(shared_quad_state->mask_filter_info.HasGradientMask());
    EXPECT_EQ(shared_quad_state->mask_filter_info.gradient_mask(), gradient);
  }

  {
    auto* quad = pass->quad_list.ElementAt(1u);
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_TRUE(shared_quad_state->mask_filter_info.HasGradientMask());
    EXPECT_EQ(shared_quad_state->mask_filter_info.gradient_mask(), gradient);
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, GradientMaskOnParentAndChild) {
  auto root_layer = CreateSolidColorLayer(viewport_.size(), SkColors::kGray);
  layer_tree_->SetRoot(root_layer);

  gfx::LinearGradient parent_gradient;
  parent_gradient.AddStep(0.0f, 255);
  parent_gradient.AddStep(1.0f, 0);
  auto parent = CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kRed);
  parent->SetGradientMask(parent_gradient);
  parent->SetPosition(gfx::PointF(10.0f, 10.0f));
  root_layer->AddChild(parent);

  gfx::LinearGradient child_gradient;
  child_gradient.AddStep(0.0f, 0);
  child_gradient.AddStep(1.0f, 255);
  auto child = CreateSolidColorLayer(gfx::Size(50, 50), SkColors::kBlue);
  child->SetPosition(gfx::PointF(10.0f, 10.0f));
  child->SetGradientMask(child_gradient);
  parent->AddChild(child);

  viz::CompositorFrame frame = ProduceFrame();
  ASSERT_EQ(frame.render_pass_list.size(), 2u);

  auto& child_pass = frame.render_pass_list.front();
  ASSERT_THAT(child_pass->quad_list,
              ElementsAre(AllOf(viz::IsSolidColorQuad(SkColors::kBlue),
                                viz::HasRect(gfx::Rect(50, 50)),
                                viz::HasVisibleRect(gfx::Rect(40, 40)),
                                viz::HasTransform(gfx::Transform()))));
  {
    auto* quad = child_pass->quad_list.front();
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_TRUE(shared_quad_state->mask_filter_info.HasGradientMask());
    EXPECT_EQ(shared_quad_state->mask_filter_info.gradient_mask(),
              child_gradient);
  }

  auto& root_pass = frame.render_pass_list.back();
  ASSERT_THAT(
      root_pass->quad_list,
      ElementsAre(
          AllOf(
              viz::IsCompositorRenderPassQuad(child_pass->id),
              viz::HasRect(gfx::Rect(40, 40)),
              viz::HasVisibleRect(gfx::Rect(40, 40)),
              viz::HasTransform(gfx::Transform::MakeTranslation(20.0f, 20.0f))),
          AllOf(
              viz::IsSolidColorQuad(SkColors::kRed),
              viz::HasRect(gfx::Rect(50, 50)),
              viz::HasTransform(gfx::Transform::MakeTranslation(10.0f, 10.0f))),
          AllOf(viz::IsSolidColorQuad(SkColors::kGray), viz::HasRect(viewport_),
                viz::HasVisibleRect(viewport_),
                viz::HasTransform(gfx::Transform()))));
  {
    auto* quad = root_pass->quad_list.front();
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_TRUE(shared_quad_state->mask_filter_info.HasGradientMask());
    EXPECT_EQ(shared_quad_state->mask_filter_info.gradient_mask(),
              parent_gradient);
  }

  {
    auto* quad = root_pass->quad_list.ElementAt(1u);
    auto* shared_quad_state = quad->shared_quad_state;
    EXPECT_TRUE(shared_quad_state->mask_filter_info.HasGradientMask());
    EXPECT_EQ(shared_quad_state->mask_filter_info.gradient_mask(),
              parent_gradient);
  }
}

// Testing that {Add|Remove}SurfaceRange should trigger a draw via
// `SetNeedsDraw`, where the added or removed surface range should be reflected
// in the metadata of the next frame's metadata.
TEST_F(SlimLayerTreeCompositorFrameTest,
       AddRemoveSurfaceRangesTriggerSetNeedsDraw) {
  auto surface_layer = SurfaceLayer::Create();
  surface_layer->SetBounds(viewport_.size());
  surface_layer->SetIsDrawable(true);
  surface_layer->SetContentsOpaque(true);
  layer_tree_->SetRoot(surface_layer);

  base::UnguessableToken token = base::UnguessableToken::Create();
  viz::SurfaceId start(viz::FrameSinkId(1u, 2u),
                       viz::LocalSurfaceId(3u, 4u, token));
  viz::SurfaceId end(viz::FrameSinkId(1u, 2u),
                     viz::LocalSurfaceId(5u, 6u, token));
  cc::DeadlinePolicy deadline_policy = cc::DeadlinePolicy::UseDefaultDeadline();
  surface_layer->SetOldestAcceptableFallback(start);
  surface_layer->SetSurfaceId(end, deadline_policy);

  // Add/remove a SurfaceRange different from the one of the `surface_layer`.
  {
    layer_tree_->AddSurfaceRange(viz::SurfaceRange(end, end));
    const viz::CompositorFrame frame = ProduceFrame();
    EXPECT_THAT(frame.metadata.referenced_surfaces,
                testing::UnorderedElementsAre(viz::SurfaceRange(start, end),
                                              viz::SurfaceRange(end, end)));
  }
  {
    layer_tree_->RemoveSurfaceRange(viz::SurfaceRange(end, end));
    const viz::CompositorFrame frame = ProduceFrame();
    EXPECT_THAT(frame.metadata.referenced_surfaces,
                testing::UnorderedElementsAre(viz::SurfaceRange(start, end)));
  }

  // Add/remove a SurfaceRange that's the same as the one of the
  // `surface_layer`. Since the ranges are the same, only one range entry is
  // referenced in the metadata.
  {
    layer_tree_->AddSurfaceRange(viz::SurfaceRange(start, end));

    const viz::CompositorFrame frame = ProduceFrame();
    EXPECT_THAT(frame.metadata.referenced_surfaces,
                testing::UnorderedElementsAre(viz::SurfaceRange(start, end)));
  }
  {
    layer_tree_->RemoveSurfaceRange(viz::SurfaceRange(start, end));
    const viz::CompositorFrame frame = ProduceFrame();
    EXPECT_THAT(frame.metadata.referenced_surfaces,
                testing::UnorderedElementsAre(viz::SurfaceRange(start, end)));
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, OffsetTagLayers) {
  layer_tree_->set_background_color(SkColors::kGreen);
  auto root_layer = Layer::Create();
  layer_tree_->SetRoot(root_layer);

  auto background_layer = SolidColorLayer::Create();
  background_layer->SetBounds(viewport_.size());
  background_layer->SetBackgroundColor(SkColors::kBlack);
  background_layer->SetIsDrawable(true);
  root_layer->AddChild(background_layer);

  auto container_layer = Layer::Create();
  root_layer->AddChild(container_layer);

  auto solid_color_layer = SolidColorLayer::Create();
  solid_color_layer->SetBounds(viewport_.size());
  solid_color_layer->SetBackgroundColor(SkColors::kRed);
  solid_color_layer->SetIsDrawable(true);
  container_layer->AddChild(solid_color_layer);

  auto surface_layer = SurfaceLayer::Create();
  surface_layer->SetBounds(viewport_.size());
  const base::UnguessableToken token = base::UnguessableToken::Create();
  const viz::FrameSinkId frame_sink_id(1u, 2u);
  const viz::SurfaceId surface_id(frame_sink_id,
                                  viz::LocalSurfaceId(3u, 4u, token));
  surface_layer->SetSurfaceId(surface_id,
                              cc::DeadlinePolicy::UseDefaultDeadline());
  surface_layer->SetIsDrawable(true);
  container_layer->AddChild(surface_layer);

  {
    // Draw the first frame. There are no OffsetTags added yet.
    const viz::CompositorFrame frame = ProduceFrame();
    EXPECT_THAT(frame.metadata.offset_tag_definitions, testing::SizeIs(0));

    ASSERT_THAT(frame.render_pass_list, testing::SizeIs(1));
    auto* root_pass = frame.render_pass_list[0].get();

    EXPECT_THAT(root_pass->quad_list,
                testing::ElementsAre(
                    // Quad for `surface_layer`.
                    testing::AllOf(viz::IsSurfaceQuad(), viz::HasOffsetTag({})),
                    // Quad for `solid_color_layer`. This is opaque and it
                    // totally occludes `background_layer` so it's culled.
                    testing::AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                   viz::HasOffsetTag({}))));
  }

  const auto offset_tag = viz::OffsetTag::CreateRandom();
  const viz::OffsetTagConstraints constraints(0, 0, -10.0f, 0);
  surface_layer->RegisterOffsetTag(offset_tag, constraints);
  container_layer->SetOffsetTag(offset_tag);

  {
    // Add OffsetTag to `container_layer` so that it applies to subtree from
    // there. There will be one OffsetTagDefinition now and quads for
    // `surface_layer` and `solid_color_layer` will be tagged.
    const viz::CompositorFrame frame = ProduceFrame();
    EXPECT_THAT(frame.metadata.offset_tag_definitions, testing::SizeIs(1));
    auto& tag_def = frame.metadata.offset_tag_definitions[0];
    EXPECT_EQ(tag_def.provider, viz::SurfaceRange(std::nullopt, surface_id));
    EXPECT_EQ(tag_def.tag, offset_tag);

    ASSERT_THAT(frame.render_pass_list, testing::SizeIs(1));
    auto* root_pass = frame.render_pass_list[0].get();

    EXPECT_THAT(
        root_pass->quad_list,
        testing::ElementsAre(
            // Quad for `surface_layer`.
            testing::AllOf(viz::IsSurfaceQuad(), viz::HasOffsetTag(offset_tag)),
            // Quad for `solid_color_layer`.
            testing::AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                           viz::HasOffsetTag(offset_tag)),
            // Quad for `background_layer`. Since `solid_color_layer` has an
            // offset tag background layer is no longer occluded.
            testing::AllOf(viz::IsSolidColorQuad(SkColors::kBlack),
                           viz::HasOffsetTag({}))));
  }

  background_layer->SetIsDrawable(false);

  {
    // Stop `background_layer` from drawing. Since all drawable layers are
    // tagged and it's not known where they draw, slim compositor will add a
    // layer tree background color SolidColorDrawQuad automatically to ensure
    // root render pass is opaque.
    const viz::CompositorFrame frame = ProduceFrame();
    ASSERT_THAT(frame.render_pass_list, testing::SizeIs(1));
    auto* root_pass = frame.render_pass_list[0].get();

    EXPECT_THAT(
        root_pass->quad_list,
        testing::ElementsAre(
            // Quad for `surface_layer`.
            testing::AllOf(viz::IsSurfaceQuad(), viz::HasOffsetTag(offset_tag)),
            // Quad for `solid_color_layer`.
            testing::AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                           viz::HasOffsetTag(offset_tag)),
            // Generated background quad that's green since `solid_color_layer`
            // no longer counts as opaque and `background_layer` isn't drawn.
            testing::AllOf(viz::IsSolidColorQuad(SkColors::kGreen),
                           viz::HasOffsetTag({}))));
  }

  auto rotated_layer = Layer::Create();
  rotated_layer->SetBounds(gfx::Size(10, 10));
  {
    // Rotate 45 degrees on center of layer and then translate 15, 15.
    gfx::Transform transform = gfx::Transform::MakeTranslation(-5.0f, -5.0f);
    transform.Rotate(-45.0);
    transform.Translate(20.0f, 20.0f);
    rotated_layer->SetTransform(transform);
  }
  rotated_layer->SetMasksToBounds(true);
  solid_color_layer->AddChild(rotated_layer);

  auto rotated_color_layer = SolidColorLayer::Create();
  // This size is bigger than `rotated_layer` so it will be clipped.
  rotated_color_layer->SetBounds(gfx::Size(100, 100));
  rotated_color_layer->SetBackgroundColor(SkColors::kBlue);
  rotated_color_layer->SetIsDrawable(true);
  rotated_layer->AddChild(rotated_color_layer);

  {
    // Add a rotated container layer that masks to bounds and solid color layer
    // inside of it. This will produce another render pass to do clipping in the
    // rotated layer coordinate space. Make sure the RenderPassDrawQuad has
    // the OffsetTag but not the SolidColorDrawQuad in the new render pass.
    const viz::CompositorFrame frame = ProduceFrame();
    ASSERT_THAT(frame.render_pass_list, testing::SizeIs(2));
    auto* child_pass = frame.render_pass_list[0].get();
    auto* root_pass = frame.render_pass_list[1].get();

    EXPECT_THAT(
        root_pass->quad_list,
        testing::ElementsAre(
            // Quad for `surface_layer`.
            testing::AllOf(viz::IsSurfaceQuad(), viz::HasOffsetTag(offset_tag)),
            // Quad for `rotated_layer` ends up in a new render pass.
            testing::AllOf(viz::IsCompositorRenderPassQuad(child_pass->id),
                           viz::HasOffsetTag(offset_tag)),
            // Quad for `solid_color_layer`.
            testing::AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                           viz::HasOffsetTag(offset_tag)),
            // Generated background quad that's green since `solid_color_layer`
            // no longer counts as opaque and `background_layer` isn't drawn.
            testing::AllOf(viz::IsSolidColorQuad(SkColors::kGreen),
                           viz::HasOffsetTag({}))));

    EXPECT_THAT(
        child_pass->quad_list,
        testing::ElementsAre(
            // Quad for `rotated_color_layer`. It's clipped to `rotated_quad`
            // size using visible_rect and it doesn't have an OffsetTag
            // since that was already applied to the RenderPassDrawQuad.
            testing::AllOf(viz::IsSolidColorQuad(SkColors::kBlue),
                           viz::HasVisibleRect(gfx::Rect(10, 10)),
                           viz::HasOffsetTag({}))));
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, OffsetTagVisibleRect) {
  layer_tree_->set_background_color(SkColors::kTransparent);

  auto root_layer = Layer::Create();
  layer_tree_->SetRoot(root_layer);

  auto surface_layer = SurfaceLayer::Create();
  surface_layer->SetBounds(viewport_.size());
  const viz::FrameSinkId frame_sink_id(1u, 2u);
  const viz::SurfaceId surface_id(
      frame_sink_id,
      viz::LocalSurfaceId(3u, 4u, base::UnguessableToken::Create()));
  surface_layer->SetSurfaceId(surface_id,
                              cc::DeadlinePolicy::UseDefaultDeadline());
  surface_layer->SetIsDrawable(true);
  root_layer->AddChild(surface_layer);

  // This layer is outside the viewport so it's visible_rect will be clipped
  // by the viewport / root render pass output_rect.
  auto outside_viewport_layer = SolidColorLayer::Create();
  outside_viewport_layer->SetBounds(gfx::Size(150, 150));
  outside_viewport_layer->SetTransform(
      gfx::Transform::MakeTranslation(-10, -10));
  outside_viewport_layer->SetBackgroundColor(SkColors::kRed);
  outside_viewport_layer->SetIsDrawable(true);
  root_layer->AddChild(outside_viewport_layer);

  {
    const viz::CompositorFrame frame = ProduceFrame();
    ASSERT_THAT(frame.render_pass_list, testing::SizeIs(1));
    auto* root_pass = frame.render_pass_list[0].get();

    // Without an offset tag `outside_viewport_layer` is clipped by viewport.
    // `surface_layer` is also fully occluded and not included in the frame.
    EXPECT_THAT(root_pass->quad_list,
                testing::ElementsAre(testing::AllOf(
                    viz::HasVisibleRect(gfx::Rect(10, 10, 100, 100)))));
  }

  {
    const auto offset_tag = viz::OffsetTag::CreateRandom();
    const viz::OffsetTagConstraints constraints(-30, 30, -30, 30);
    surface_layer->RegisterOffsetTag(offset_tag, constraints);
    outside_viewport_layer->SetOffsetTag(offset_tag);

    const viz::CompositorFrame frame = ProduceFrame();
    ASSERT_THAT(frame.render_pass_list, testing::SizeIs(1));
    auto* root_pass = frame.render_pass_list[0].get();

    // With offset tag more of `outside_viewport_layer` will be visible,
    // depending on the OffsetTagValue, however with max shift 30 pixels up/left
    // the right and bottom pixels are still always outside viewport.
    EXPECT_THAT(root_pass->quad_list,
                testing::ElementsAre(
                    testing::AllOf(
                        viz::IsSolidColorQuad(), viz::HasOffsetTag(offset_tag),
                        viz::HasVisibleRect(gfx::Rect(0, 0, 140, 140))),
                    viz::IsSurfaceQuad()));

    auto* quad = root_pass->quad_list.ElementAt(0);
    EXPECT_EQ(quad->visible_rect, gfx::Rect(0, 0, 140, 140));
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, OffsetTagNoEmbeddedSurface) {
  layer_tree_->set_background_color(SkColors::kTransparent);

  auto root_layer = Layer::Create();
  layer_tree_->SetRoot(root_layer);

  auto surface_layer = SurfaceLayer::Create();
  surface_layer->SetBounds(viewport_.size());
  surface_layer->SetIsDrawable(true);

  root_layer->AddChild(surface_layer);

  const auto offset_tag = viz::OffsetTag::CreateRandom();
  const viz::OffsetTagConstraints constraints(-30, 30, -30, 30);
  surface_layer->RegisterOffsetTag(offset_tag, constraints);

  const viz::CompositorFrame frame = ProduceFrame();

  // Since `surface_layer` doesn't have a SurfaceId set no OffsetTagDefinition
  // is added.
  EXPECT_THAT(frame.metadata.offset_tag_definitions, testing::IsEmpty());
}

TEST_F(SlimLayerTreeCompositorFrameTest, OffsetTagClipping) {
  layer_tree_->set_background_color(SkColors::kTransparent);

  auto root_layer = Layer::Create();
  layer_tree_->SetRoot(root_layer);

  // This layer clips rect (10, 10, 25, 25) in render pass coordinate space.
  auto parent_clip_layer = Layer::Create();
  parent_clip_layer->SetBounds(gfx::Size(25, 25));
  parent_clip_layer->SetTransform(gfx::Transform::MakeTranslation(10, 10));
  parent_clip_layer->SetMasksToBounds(true);
  root_layer->AddChild(parent_clip_layer);

  auto tag_layer = Layer::Create();
  parent_clip_layer->AddChild(tag_layer);

  // This layer will clips (0, 0, 35, 35) in render pass coordinate space with
  // default tag but that will change depending on the offset value. The clip
  // has to be expressed in layer coordinate space as a result.
  auto surface_layer = SurfaceLayer::Create();
  surface_layer->SetBounds(gfx::Size(35, 35));
  surface_layer->SetTransform(gfx::Transform::MakeTranslation(-10, -10));
  surface_layer->SetMasksToBounds(true);
  const viz::FrameSinkId frame_sink_id(1u, 2u);
  const viz::SurfaceId surface_id(
      frame_sink_id,
      viz::LocalSurfaceId(3u, 4u, base::UnguessableToken::Create()));
  surface_layer->SetSurfaceId(surface_id,
                              cc::DeadlinePolicy::UseDefaultDeadline());
  surface_layer->SetIsDrawable(true);
  tag_layer->AddChild(surface_layer);

  const auto offset_tag = viz::OffsetTag::CreateRandom();
  const viz::OffsetTagConstraints constraints(-30, 30, -30, 30);
  surface_layer->RegisterOffsetTag(offset_tag, constraints);
  tag_layer->SetOffsetTag(offset_tag);

  // This layer has clipping from both `surface_layer` and `parent_clip_layer`.
  auto solid_color_layer = SolidColorLayer::Create();
  solid_color_layer->SetBounds(gfx::Size(50, 50));
  solid_color_layer->SetTransform(gfx::Transform::MakeTranslation(-5, -5));
  solid_color_layer->SetBackgroundColor(SkColors::kRed);
  solid_color_layer->SetIsDrawable(true);
  surface_layer->AddChild(solid_color_layer);

  const viz::CompositorFrame frame = ProduceFrame();
  EXPECT_THAT(frame.metadata.offset_tag_definitions, testing::SizeIs(1));

  ASSERT_THAT(frame.render_pass_list, testing::SizeIs(1));
  auto* root_pass = frame.render_pass_list[0].get();

  // This is clipping for `parent_clip_layer` which will only be done via
  // SharedQuadState::clip_rect and not DrawQuad::visible_rect.
  const gfx::Rect parent_clip_rect(10, 10, 25, 25);

  EXPECT_THAT(root_pass->quad_list,
              testing::ElementsAre(
                  // Quad for `solid_color_layer`. This has both `surface_layer`
                  // and `parent_clip_layer` applied.
                  testing::AllOf(viz::IsSolidColorQuad(SkColors::kRed),
                                 viz::HasOffsetTag(offset_tag),
                                 viz::HasVisibleRect(gfx::Rect(5, 5, 35, 35)),
                                 viz::HasClipRect(parent_clip_rect)),
                  // Quad for `surface_layer`. This only has clipping from
                  // `parent_clip_layer` applied via `clip_rect`.
                  testing::AllOf(
                      viz::IsSurfaceQuad(), viz::HasOffsetTag(offset_tag),
                      viz::HasVisibleRect(gfx::Rect(surface_layer->bounds())),
                      viz::HasClipRect(parent_clip_rect))));
}

}  // namespace

}  // namespace cc::slim
