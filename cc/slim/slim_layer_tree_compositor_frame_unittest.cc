// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "cc/slim/features.h"
#include "cc/slim/layer.h"
#include "cc/slim/nine_patch_layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/test_frame_sink_impl.h"
#include "cc/slim/test_layer_tree_client.h"
#include "cc/slim/test_layer_tree_impl.h"
#include "cc/slim/ui_resource_layer.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/test/draw_quad_matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc::slim {

namespace {

using testing::AllOf;
using testing::ElementsAre;

class SlimLayerTreeCompositorFrameTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSlimCompositor);
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

  viz::CompositorFrame ProduceFrame() {
    layer_tree_->SetNeedsRedraw();
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
  base::test::ScopedFeatureList scoped_feature_list_;
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

  // TODO(crbug.com/1408128): Add tests for features once implemented:
  // * reference_surfaces
  // * activation_dependencies
  // * deadline
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
    EXPECT_EQ(absl::nullopt, metadata.top_controls_visible_height);
  }

  IncrementLocalSurfaceId();
  layer_tree_->SetViewportRectAndScale(viewport_, /*device_scale_factor=*/2.0f,
                                       local_surface_id_);
  layer_tree_->set_background_color(SkColors::kBlue);
  layer_tree_->set_display_transform_hint(gfx::OVERLAY_TRANSFORM_ROTATE_90);
  layer_tree_->UpdateTopControlsVisibleHeight(5.0f);
  {
    viz::CompositorFrame frame = ProduceFrame();
    viz::CompositorFrameMetadata& metadata = frame.metadata;
    EXPECT_NE(0u, metadata.frame_token);
    EXPECT_NE(first_frame_token, metadata.frame_token);
    EXPECT_EQ(sequence_id_, metadata.begin_frame_ack.frame_id.sequence_number);
    EXPECT_EQ(2.0f, metadata.device_scale_factor);
    EXPECT_EQ(SkColors::kBlue, metadata.root_background_color);
    EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_90,
              metadata.display_transform_hint);
    EXPECT_EQ(5.0f, metadata.top_controls_visible_height);
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
                        viz::HasTransform(gfx::Transform()))));
  auto* quad = pass->quad_list.back();
  auto* shared_quad_state = quad->shared_quad_state;

  EXPECT_EQ(shared_quad_state->quad_layer_rect, viewport_);
  EXPECT_EQ(shared_quad_state->visible_quad_layer_rect, viewport_);
  EXPECT_EQ(shared_quad_state->clip_rect, absl::nullopt);
  EXPECT_EQ(shared_quad_state->are_contents_opaque, true);
  EXPECT_EQ(shared_quad_state->opacity, 1.0f);
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
  child->SetTransformOrigin(gfx::Point3F(5.0f, 10.0f, 0.0f));
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

  absl::optional<gfx::PresentationFeedback> feedback_opt_1;
  absl::optional<gfx::PresentationFeedback> feedback_opt_2;
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

  absl::optional<gfx::PresentationFeedback> feedback_opt_1;
  layer_tree_->RequestPresentationTimeForNextFrame(base::BindLambdaForTesting(
      [&](const gfx::PresentationFeedback& feedback) {
        feedback_opt_1 = feedback;
      }));
  viz::CompositorFrame frame1 = ProduceFrame();

  absl::optional<gfx::PresentationFeedback> feedback_opt_2;
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

  absl::optional<base::TimeTicks> feedback_time_opt_1;
  absl::optional<base::TimeTicks> feedback_time_opt_2;
  layer_tree_->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting(
          [&](base::TimeTicks timeticks) { feedback_time_opt_1 = timeticks; }));
  layer_tree_->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting(
          [&](base::TimeTicks timeticks) { feedback_time_opt_2 = timeticks; }));
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

  absl::optional<base::TimeTicks> feedback_time_opt_1;
  layer_tree_->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting(
          [&](base::TimeTicks timeticks) { feedback_time_opt_1 = timeticks; }));
  viz::CompositorFrame frame1 = ProduceFrame();
  viz::CompositorFrame frame2 = ProduceFrame();

  absl::optional<base::TimeTicks> feedback_time_opt_2;
  layer_tree_->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting(
          [&](base::TimeTicks timeticks) { feedback_time_opt_2 = timeticks; }));
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
    EXPECT_EQ(1.0f, texture_quad->vertex_opacity[0]);
    EXPECT_EQ(1.0f, texture_quad->vertex_opacity[1]);
    EXPECT_EQ(1.0f, texture_quad->vertex_opacity[2]);
    EXPECT_EQ(1.0f, texture_quad->vertex_opacity[3]);

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
  ui_resource_layer->SetVertexOpacity(0.1f, 0.2f, 0.3f, 0.4f);
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
    EXPECT_EQ(0.1f, texture_quad->vertex_opacity[0]);
    EXPECT_EQ(0.2f, texture_quad->vertex_opacity[1]);
    EXPECT_EQ(0.3f, texture_quad->vertex_opacity[2]);
    EXPECT_EQ(0.4f, texture_quad->vertex_opacity[3]);

    ASSERT_EQ(frame.resource_list.size(), 1u);
    EXPECT_EQ(frame.resource_list[0].id, texture_quad->resource_id());
    EXPECT_EQ(frame.resource_list[0].size, gfx::Size(2, 2));
    EXPECT_NE(first_resource_id, texture_quad->resource_id());
  }
}

TEST_F(SlimLayerTreeCompositorFrameTest, NinePatchLayerAppendQuads) {
  auto nine_patch_layer = NinePatchLayer::Create();
  nine_patch_layer->SetBounds(viewport_.size());
  nine_patch_layer->SetIsDrawable(true);
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
    EXPECT_EQ(1.0f, texture_quad->vertex_opacity[0]);
    EXPECT_EQ(1.0f, texture_quad->vertex_opacity[1]);
    EXPECT_EQ(1.0f, texture_quad->vertex_opacity[2]);
    EXPECT_EQ(1.0f, texture_quad->vertex_opacity[3]);

    EXPECT_EQ(frame.resource_list[0].id, texture_quad->resource_id());
    EXPECT_EQ(frame_sink_->uploaded_resources().begin()->second.viz_resource_id,
              texture_quad->resource_id());
  }
}

}  // namespace

}  // namespace cc::slim
