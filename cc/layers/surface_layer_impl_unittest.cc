// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer_impl.h"

#include <stddef.h>
#include <utility>

#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "cc/layers/append_quads_data.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace cc {
namespace {

static constexpr viz::FrameSinkId kArbitraryFrameSinkId(1, 1);

TEST(SurfaceLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId(
      9, base::UnguessableToken::Create());

  LayerTreeImplTestBase impl;

  SurfaceLayerImpl* surface_layer_impl =
      impl.AddLayerInActiveTree<SurfaceLayerImpl>();
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  viz::SurfaceId surface_id(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId);
  surface_layer_impl->SetRange(viz::SurfaceRange(std::nullopt, surface_id),
                               std::nullopt);
  CopyProperties(impl.root_layer(), surface_layer_impl);

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(surface_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect(layer_size));
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_TRUE(surface_layer_impl->WillDraw(DRAW_MODE_HARDWARE, nullptr));
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(surface_layer_impl->visible_layer_rect());
    impl.AppendQuadsWithOcclusion(surface_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
    EXPECT_FALSE(surface_layer_impl->WillDraw(DRAW_MODE_HARDWARE, nullptr));
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendQuadsWithOcclusion(surface_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    VerifyQuadsAreOccluded(impl.quad_list(), occluded,
                           &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
    EXPECT_TRUE(surface_layer_impl->WillDraw(DRAW_MODE_HARDWARE, nullptr));
  }
}

// This test verifies that activation_dependencies and the fallback_surface_id
// are populated correctly if primary and fallback surfaces differ.
TEST(SurfaceLayerImplTest, SurfaceLayerImplWithTwoDifferentSurfaces) {
  LayerTreeImplTestBase impl;
  SurfaceLayerImpl* surface_layer_impl =
      impl.AddLayerInActiveTree<SurfaceLayerImpl>();

  // Populate the primary viz::SurfaceInfo.
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId1(
      9, base::UnguessableToken::Create());
  viz::SurfaceId surface_id1(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId1);

  // Populate the fallback viz::SurfaceId.
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId2(
      7, kArbitraryLocalSurfaceId1.embed_token());
  viz::SurfaceId surface_id2(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId2);

  gfx::Size layer_size(400, 100);

  // Populate the SurfaceLayerImpl ensuring that the primary and fallback
  // SurfaceInfos are different.
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  surface_layer_impl->SetRange(viz::SurfaceRange(surface_id2, surface_id1), 2u);
  surface_layer_impl->SetBackgroundColor(SkColors::kBlue);
  CopyProperties(impl.root_layer(), surface_layer_impl);

  gfx::Size viewport_size(1000, 1000);
  impl.CalcDrawProps(viewport_size);

  auto render_pass = viz::CompositorRenderPass::Create();
  {
    AppendQuadsData data;
    surface_layer_impl->AppendQuads(render_pass.get(), &data);
    // The the primary viz::SurfaceInfo will be added to
    // activation_dependencies.
    EXPECT_THAT(data.activation_dependencies,
                UnorderedElementsAre(surface_id1));
    EXPECT_EQ(2u, data.deadline_in_frames);
    EXPECT_FALSE(data.use_default_lower_bound_deadline);
  }

  // Update the fallback to an invalid viz::SurfaceInfo. The
  // |activation_dependencies| should still contain the primary
  // viz::SurfaceInfo.
  {
    AppendQuadsData data;
    surface_layer_impl->SetRange(viz::SurfaceRange(std::nullopt, surface_id1),
                                 0u);
    surface_layer_impl->AppendQuads(render_pass.get(), &data);
    // The primary viz::SurfaceInfo should be added to activation_dependencies.
    EXPECT_THAT(data.activation_dependencies,
                UnorderedElementsAre(surface_id1));
    EXPECT_EQ(0u, data.deadline_in_frames);
    EXPECT_FALSE(data.use_default_lower_bound_deadline);
  }

  // Update the primary deadline and fallback viz::SurfaceId and
  // re-emit DrawQuads.
  {
    AppendQuadsData data;
    surface_layer_impl->SetRange(viz::SurfaceRange(surface_id2, surface_id1),
                                 4u);
    surface_layer_impl->AppendQuads(render_pass.get(), &data);
    // The the primary viz::SurfaceInfo will be added to
    // activation_dependencies.
    EXPECT_THAT(data.activation_dependencies,
                UnorderedElementsAre(surface_id1));
    // The primary SurfaceId hasn't changed but a new deadline was explicitly
    // requested in SetRange so we'll use it in the next CompositorFrame.
    EXPECT_EQ(4u, data.deadline_in_frames);
    EXPECT_FALSE(data.use_default_lower_bound_deadline);
  }

  ASSERT_EQ(3u, render_pass->quad_list.size());
  const viz::SurfaceDrawQuad* surface_draw_quad1 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(0));
  ASSERT_TRUE(surface_draw_quad1);
  const viz::SurfaceDrawQuad* surface_draw_quad2 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(1));
  ASSERT_TRUE(surface_draw_quad2);
  const viz::SurfaceDrawQuad* surface_draw_quad3 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(2));
  ASSERT_TRUE(surface_draw_quad3);

  EXPECT_EQ(surface_id1, surface_draw_quad1->surface_range.end());
  EXPECT_EQ(SkColors::kBlue, surface_draw_quad1->default_background_color);
  EXPECT_EQ(surface_id2, surface_draw_quad1->surface_range.start());

  EXPECT_EQ(surface_id1, surface_draw_quad2->surface_range.end());
  EXPECT_EQ(SkColors::kBlue, surface_draw_quad2->default_background_color);
  EXPECT_EQ(std::nullopt, surface_draw_quad2->surface_range.start());

  EXPECT_EQ(surface_id1, surface_draw_quad3->surface_range.end());
  EXPECT_EQ(SkColors::kBlue, surface_draw_quad3->default_background_color);
  EXPECT_EQ(surface_id2, surface_draw_quad3->surface_range.start());
}

// This test verifies that if one SurfaceLayerImpl has a deadline
// and the other uses the default then AppendQuadsData is populated
// correctly.
TEST(SurfaceLayerImplTest, SurfaceLayerImplsWithDeadlines) {
  LayerTreeImplTestBase impl;
  SurfaceLayerImpl* surface_layer_impl =
      impl.AddLayerInActiveTree<SurfaceLayerImpl>();
  CopyProperties(impl.root_layer(), surface_layer_impl);

  SurfaceLayerImpl* surface_layer_impl2 =
      impl.AddLayerInActiveTree<SurfaceLayerImpl>();
  CopyProperties(impl.root_layer(), surface_layer_impl2);

  const viz::LocalSurfaceId kArbitraryLocalSurfaceId1(
      1, base::UnguessableToken::Create());
  viz::SurfaceId surface_id1(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId1);

  const viz::LocalSurfaceId kArbitraryLocalSurfaceId2(
      2, kArbitraryLocalSurfaceId1.embed_token());
  viz::SurfaceId surface_id2(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId2);

  gfx::Size viewport_size(1000, 1000);
  impl.CalcDrawProps(viewport_size);

  gfx::Size layer_size(400, 100);

  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  surface_layer_impl->SetRange(viz::SurfaceRange(surface_id1, surface_id2), 1u);

  surface_layer_impl2->SetBounds(layer_size);
  surface_layer_impl2->SetDrawsContent(true);
  surface_layer_impl2->SetRange(viz::SurfaceRange(surface_id1, surface_id2),
                                std::nullopt);

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  surface_layer_impl->AppendQuads(render_pass.get(), &data);
  EXPECT_EQ(1u, data.deadline_in_frames);
  EXPECT_FALSE(data.use_default_lower_bound_deadline);

  surface_layer_impl2->AppendQuads(render_pass.get(), &data);
  EXPECT_EQ(1u, data.deadline_in_frames);
  EXPECT_TRUE(data.use_default_lower_bound_deadline);
}

// This test verifies that one viz::SurfaceDrawQuad is emitted if a
// SurfaceLayerImpl holds the same surface ID for both the primary
// and fallback viz::SurfaceInfo.
TEST(SurfaceLayerImplTest, SurfaceLayerImplWithMatchingPrimaryAndFallback) {
  LayerTreeImplTestBase impl;
  SurfaceLayerImpl* surface_layer_impl =
      impl.AddLayerInActiveTree<SurfaceLayerImpl>();

  // Populate the primary viz::SurfaceId.
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId1(
      9, base::UnguessableToken::Create());
  viz::SurfaceId surface_id1(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId1);

  gfx::Size layer_size(400, 100);

  // Populate the SurfaceLayerImpl ensuring that the primary and fallback
  // SurfaceInfos are the same.
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  surface_layer_impl->SetRange(viz::SurfaceRange(surface_id1), 1u);
  surface_layer_impl->SetRange(viz::SurfaceRange(surface_id1), 2u);
  surface_layer_impl->SetBackgroundColor(SkColors::kBlue);
  CopyProperties(impl.root_layer(), surface_layer_impl);

  gfx::Size viewport_size(1000, 1000);
  impl.CalcDrawProps(viewport_size);

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  surface_layer_impl->AppendQuads(render_pass.get(), &data);
  EXPECT_THAT(data.activation_dependencies, UnorderedElementsAre(surface_id1));
  EXPECT_EQ(2u, data.deadline_in_frames);

  ASSERT_EQ(1u, render_pass->quad_list.size());
  const viz::SurfaceDrawQuad* surface_draw_quad1 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(0));
  ASSERT_TRUE(surface_draw_quad1);

  EXPECT_EQ(surface_id1, surface_draw_quad1->surface_range.end());
  EXPECT_EQ(surface_id1, surface_draw_quad1->surface_range.start());
  EXPECT_EQ(SkColors::kBlue, surface_draw_quad1->default_background_color);
}

TEST(SurfaceLayerImplTest, GetEnclosingRectInTargetSpace) {
  gfx::Size layer_size(902, 1000);
  gfx::Size viewport_size(902, 1000);
  LayerTreeImplTestBase impl;
  SurfaceLayerImpl* surface_layer_impl =
      impl.AddLayerInActiveTree<SurfaceLayerImpl>();
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  CopyProperties(impl.root_layer(), surface_layer_impl);

  // A device scale of 1.33 and transform of 1.5 were chosen as they produce
  // different results when rounding at each stage, vs applying a single
  // transform.
  gfx::Transform transform;
  transform.Scale(1.5, 1.5);
  impl.host_impl()->active_tree()->SetDeviceScaleFactor(1.33);
  impl.CalcDrawProps(viewport_size);
  surface_layer_impl->draw_properties().target_space_transform = transform;

  // GetEnclosingRectInTargetSpace() and GetScaledEnclosingRectInTargetSpace()
  // should return the same value, otherwise we may not damage the right
  // pixels.
  EXPECT_EQ(
      surface_layer_impl->GetScaledEnclosingVisibleRectInTargetSpace(1.33),
      surface_layer_impl->GetEnclosingVisibleRectInTargetSpace());
}

TEST(SurfaceLayerImplTest, WillDrawNotifiesSynchronouslyInCompositeImmediate) {
  auto thread = std::make_unique<base::Thread>("VideoCompositor");
  ASSERT_TRUE(thread->StartAndWaitForTesting());
  scoped_refptr<base::TaskRunner> task_runner = thread->task_runner();

  bool updated = false;
  UpdateSubmissionStateCB callback = base::BindLambdaForTesting(
      [task_runner, &updated](bool draw, base::WaitableEvent* done) {
        // SurfaceLayerImpl also notifies the callback on destruction with draw
        // = false. Ignore that call, since it's not really a part of the test.
        if (!draw)
          return;
        // We're going to return control to the compositor, without signalling
        // |done| for a bit.
        task_runner->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                [](bool* updated, base::WaitableEvent* done) {
                  *updated = true;
                  if (done)
                    done->Signal();
                },
                base::Unretained(&updated), base::Unretained(done)),
            base::Milliseconds(100));
      });

  // Note that this has to be created after the callback so that the layer is
  // destroyed first (it will call the callback in the dtor).
  LayerTreeImplTestBase impl;
  impl.host_impl()->client()->set_is_synchronous_composite(true);

  SurfaceLayerImpl* surface_layer_impl =
      impl.AddLayerInActiveTree<SurfaceLayerImpl>(std::move(callback));
  surface_layer_impl->SetBounds(gfx::Size(500, 500));
  surface_layer_impl->SetDrawsContent(true);

  CopyProperties(impl.root_layer(), surface_layer_impl);
  impl.CalcDrawProps(gfx::Size(500, 500));

  surface_layer_impl->WillDraw(DRAW_MODE_SOFTWARE, nullptr);

  // This would not be set to true if WillDraw() completed before the task
  // posted by `callback` runs and completes. That task is posted with a delay
  // to ensure that the current thread really did wait.
  EXPECT_TRUE(updated);
}

TEST(SurfaceLayerImplTest, WillDrawNotifiesAsynchronously) {
  bool updated = false;
  UpdateSubmissionStateCB callback = base::BindLambdaForTesting(
      [&updated](bool draw, base::WaitableEvent* done) {
        // SurfaceLayerImpl also notifies the callback on destruction with draw
        // = false. Ignore that call, since it's not really a part of the test.
        if (!draw)
          return;

        // Note that the event should always be null here, meaning we don't
        // synchronize anything if posted across threads. This is because we're
        // using threaded compositing in this test (see
        // set_is_synchronous_composite(false) call below).
        EXPECT_FALSE(done);
        updated = true;
      });

  // Note that this has to be created after the callback so that the layer is
  // destroyed first (it will call the callback in the dtor).
  LayerTreeImplTestBase impl;
  impl.host_impl()->client()->set_is_synchronous_composite(false);

  SurfaceLayerImpl* surface_layer_impl =
      impl.AddLayerInActiveTree<SurfaceLayerImpl>(std::move(callback));
  surface_layer_impl->SetBounds(gfx::Size(500, 500));
  surface_layer_impl->SetDrawsContent(true);

  CopyProperties(impl.root_layer(), surface_layer_impl);
  impl.CalcDrawProps(gfx::Size(500, 500));

  surface_layer_impl->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  // We should have called the callback, which would set `updated` to true.
  EXPECT_TRUE(updated);
}

}  // namespace
}  // namespace cc
