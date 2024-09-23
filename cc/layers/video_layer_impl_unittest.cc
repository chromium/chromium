// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/video_layer_impl.h"

#include <stddef.h>

#include "base/functional/callback_helpers.h"
#include "cc/layers/video_frame_provider_client_impl.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/output_surface.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

// NOTE: We cannot use DebugScopedSetImplThreadAndMainThreadBlocked in these
// tests because it gets destroyed before the VideoLayerImpl is destroyed. This
// causes a DCHECK in VideoLayerImpl's destructor to fail.
static void DebugSetImplThreadAndMainThreadBlocked(
    TaskRunnerProvider* task_runner_provider) {
#if DCHECK_IS_ON()
  task_runner_provider->SetCurrentThreadIsImplThread(true);
  task_runner_provider->SetMainThreadBlocked(true);
#endif
}

TEST(VideoLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTreeImplTestBase impl;
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());

  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, gfx::Size(10, 10), gfx::Rect(10, 10),
      gfx::Size(10, 10), base::TimeDelta());
  FakeVideoFrameProvider provider;
  provider.set_frame(video_frame);

  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_0);
  video_layer_impl->SetBounds(layer_size);
  video_layer_impl->SetDrawsContent(true);
  video_layer_impl->set_visible_layer_rect(gfx::Rect(layer_size));
  CopyProperties(impl.root_layer(), video_layer_impl);

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect(layer_size));

    VerifyQuadsExactlyCoverRect(
        impl.quad_list(),
        impl.quad_list().cbegin()->shared_quad_state->visible_quad_layer_rect);
    EXPECT_EQ(1u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(video_layer_impl->visible_layer_rect());
    impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    VerifyQuadsAreOccluded(impl.quad_list(), occluded,
                           &partially_occluded_count);
    VerifyQuadsExactlyCoverRect(
        impl.quad_list(),
        impl.quad_list().cbegin()->shared_quad_state->visible_quad_layer_rect);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
  }
}

TEST(VideoLayerImplTest, OccludesOtherLayers) {
  gfx::Size layer_size(1000, 1000);
  gfx::Rect visible(layer_size);

  LayerTreeImplTestBase impl;
  impl.host_impl()->active_tree()->SetDeviceViewportRect(visible);
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());
  auto* active_tree = impl.host_impl()->active_tree();

  // Create a video layer with no frame on top of another layer.
  LayerImpl* root = impl.root_layer();
  root->SetBounds(layer_size);
  root->SetDrawsContent(true);
  const auto& draw_properties = root->draw_properties();

  FakeVideoFrameProvider provider;
  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_0);
  video_layer_impl->SetBounds(layer_size);
  video_layer_impl->SetDrawsContent(true);
  video_layer_impl->SetContentsOpaque(true);
  CopyProperties(root, video_layer_impl);

  impl.CalcDrawProps(layer_size);

  // We don't have a frame yet, so the video doesn't occlude the layer below it.
  EXPECT_FALSE(draw_properties.occlusion_in_content_space.IsOccluded(visible));

  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, gfx::Size(10, 10), gfx::Rect(10, 10),
      gfx::Size(10, 10), base::TimeDelta());
  provider.set_frame(video_frame);
  active_tree->set_needs_update_draw_properties();
  active_tree->UpdateDrawProperties(
      /*update_tiles=*/true, /*update_image_animation_controller=*/true);

  // We have a frame now, so the video occludes the layer below it.
  EXPECT_TRUE(draw_properties.occlusion_in_content_space.IsOccluded(visible));
}

TEST(VideoLayerImplTest, DidBecomeActiveShouldSetActiveVideoLayer) {
  LayerTreeImplTestBase impl;
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());

  FakeVideoFrameProvider provider;
  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_0);
  CopyProperties(impl.root_layer(), video_layer_impl);

  VideoFrameProviderClientImpl* client =
      static_cast<VideoFrameProviderClientImpl*>(provider.client());
  ASSERT_TRUE(client);

  EXPECT_FALSE(client->ActiveVideoLayer());
  video_layer_impl->DidBecomeActive();
  EXPECT_EQ(video_layer_impl, client->ActiveVideoLayer());
}

TEST(VideoLayerImplTest, Rotated0) {
  gfx::Size layer_size(100, 50);
  gfx::Size viewport_size(1000, 500);

  LayerTreeImplTestBase impl;
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());

  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, gfx::Size(20, 10), gfx::Rect(20, 10),
      gfx::Size(20, 10), base::TimeDelta());
  FakeVideoFrameProvider provider;
  provider.set_frame(video_frame);

  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_0);
  video_layer_impl->SetBounds(layer_size);
  video_layer_impl->SetDrawsContent(true);
  CopyProperties(impl.root_layer(), video_layer_impl);

  impl.CalcDrawProps(viewport_size);
  gfx::Rect occluded;
  impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

  EXPECT_EQ(1u, impl.quad_list().size());

  gfx::Point3F p1(0, impl.quad_list().front()->rect.height(), 0);
  gfx::Point3F p2(impl.quad_list().front()->rect.width(), 0, 0);
  p1 = impl.quad_list()
           .front()
           ->shared_quad_state->quad_to_target_transform.MapPoint(p1);
  p2 = impl.quad_list()
           .front()
           ->shared_quad_state->quad_to_target_transform.MapPoint(p2);
  EXPECT_EQ(gfx::Point3F(0, 50, 0), p1);
  EXPECT_EQ(gfx::Point3F(100, 0, 0), p2);
}

TEST(VideoLayerImplTest, Rotated90) {
  gfx::Size layer_size(100, 50);
  gfx::Size viewport_size(1000, 500);

  LayerTreeImplTestBase impl;
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());

  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, gfx::Size(20, 10), gfx::Rect(20, 10),
      gfx::Size(20, 10), base::TimeDelta());
  FakeVideoFrameProvider provider;
  provider.set_frame(video_frame);

  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_90);
  video_layer_impl->SetBounds(layer_size);
  video_layer_impl->SetDrawsContent(true);
  CopyProperties(impl.root_layer(), video_layer_impl);

  impl.CalcDrawProps(viewport_size);
  gfx::Rect occluded;
  impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

  EXPECT_EQ(1u, impl.quad_list().size());

  gfx::Point3F p1(0, impl.quad_list().front()->rect.height(), 0);
  gfx::Point3F p2(impl.quad_list().front()->rect.width(), 0, 0);
  p1 = impl.quad_list()
           .front()
           ->shared_quad_state->quad_to_target_transform.MapPoint(p1);
  p2 = impl.quad_list()
           .front()
           ->shared_quad_state->quad_to_target_transform.MapPoint(p2);
  EXPECT_EQ(gfx::Point3F(0, 0, 0), p1);
  EXPECT_EQ(gfx::Point3F(100, 50, 0), p2);
}

TEST(VideoLayerImplTest, Rotated180) {
  gfx::Size layer_size(100, 50);
  gfx::Size viewport_size(1000, 500);

  LayerTreeImplTestBase impl;
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());

  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, gfx::Size(20, 10), gfx::Rect(20, 10),
      gfx::Size(20, 10), base::TimeDelta());
  FakeVideoFrameProvider provider;
  provider.set_frame(video_frame);

  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_180);
  video_layer_impl->SetBounds(layer_size);
  video_layer_impl->SetDrawsContent(true);
  CopyProperties(impl.root_layer(), video_layer_impl);

  impl.CalcDrawProps(viewport_size);
  gfx::Rect occluded;
  impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

  EXPECT_EQ(1u, impl.quad_list().size());

  gfx::Point3F p1 =
      impl.quad_list()
          .front()
          ->shared_quad_state->quad_to_target_transform.MapPoint(
              gfx::Point3F(0, impl.quad_list().front()->rect.height(), 0));
  gfx::Point3F p2 =
      impl.quad_list()
          .front()
          ->shared_quad_state->quad_to_target_transform.MapPoint(
              gfx::Point3F(impl.quad_list().front()->rect.width(), 0, 0));
  EXPECT_EQ(gfx::Point3F(100, 0, 0), p1);
  EXPECT_EQ(gfx::Point3F(0, 50, 0), p2);
}

TEST(VideoLayerImplTest, Rotated270) {
  gfx::Size layer_size(100, 50);
  gfx::Size viewport_size(1000, 500);

  LayerTreeImplTestBase impl;
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());

  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, gfx::Size(20, 10), gfx::Rect(20, 10),
      gfx::Size(20, 10), base::TimeDelta());
  FakeVideoFrameProvider provider;
  provider.set_frame(video_frame);

  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_270);
  video_layer_impl->SetBounds(layer_size);
  video_layer_impl->SetDrawsContent(true);
  CopyProperties(impl.root_layer(), video_layer_impl);

  impl.CalcDrawProps(viewport_size);
  gfx::Rect occluded;
  impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

  EXPECT_EQ(1u, impl.quad_list().size());

  gfx::Point3F p1 =
      impl.quad_list()
          .front()
          ->shared_quad_state->quad_to_target_transform.MapPoint(
              gfx::Point3F(0, impl.quad_list().front()->rect.height(), 0));
  gfx::Point3F p2 =
      impl.quad_list()
          .front()
          ->shared_quad_state->quad_to_target_transform.MapPoint(
              gfx::Point3F(impl.quad_list().front()->rect.width(), 0, 0));
  EXPECT_EQ(gfx::Point3F(100, 50, 0), p1);
  EXPECT_EQ(gfx::Point3F(0, 0, 0), p2);
}

TEST(VideoLayerImplTest, SoftwareVideoFrameGeneratesYUVQuad) {
  gfx::Size layer_size(1000, 1000);

  LayerTreeImplTestBase impl;
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());

  gpu::MailboxHolder mailbox_holder;
  mailbox_holder.mailbox.name[0] = 1;

  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, gfx::Size(20, 10), gfx::Rect(20, 10),
      gfx::Size(20, 10), base::TimeDelta());

  FakeVideoFrameProvider provider;
  provider.set_frame(video_frame);

  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_0);
  video_layer_impl->SetBounds(layer_size);
  video_layer_impl->SetDrawsContent(true);
  video_layer_impl->set_visible_layer_rect(gfx::Rect(layer_size));
  CopyProperties(impl.root_layer(), video_layer_impl);

  impl.CalcDrawProps(layer_size);

  gfx::Rect occluded;
  impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

  EXPECT_EQ(1u, impl.quad_list().size());
  const viz::DrawQuad* draw_quad = impl.quad_list().ElementAt(0);

  ASSERT_EQ(viz::DrawQuad::Material::kTextureContent, draw_quad->material);
  const auto* texture_draw_quad =
      static_cast<const viz::TextureDrawQuad*>(draw_quad);
  EXPECT_TRUE(texture_draw_quad->is_video_frame);
}

TEST(VideoLayerImplTest, HibitSoftwareVideoFrameGeneratesYUVQuad) {
  gfx::Size layer_size(1000, 1000);

  LayerTreeImplTestBase impl;
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());

  gpu::MailboxHolder mailbox_holder;
  mailbox_holder.mailbox.name[0] = 1;

  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_YUV420P10, gfx::Size(20, 10), gfx::Rect(20, 10),
      gfx::Size(20, 10), base::TimeDelta());

  FakeVideoFrameProvider provider;
  provider.set_frame(video_frame);

  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_0);
  video_layer_impl->SetBounds(layer_size);
  video_layer_impl->SetDrawsContent(true);
  video_layer_impl->set_visible_layer_rect(gfx::Rect(layer_size));
  CopyProperties(impl.root_layer(), video_layer_impl);

  impl.CalcDrawProps(layer_size);

  gfx::Rect occluded;
  impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

  EXPECT_EQ(1u, impl.quad_list().size());
  const viz::DrawQuad* draw_quad = impl.quad_list().ElementAt(0);

  ASSERT_EQ(viz::DrawQuad::Material::kTextureContent, draw_quad->material);
  const auto* texture_draw_quad =
      static_cast<const viz::TextureDrawQuad*>(draw_quad);
  EXPECT_TRUE(texture_draw_quad->is_video_frame);
}

TEST(VideoLayerImplTest, NativeYUVFrameGeneratesYUVQuad) {
  gfx::Size layer_size(1000, 1000);

  LayerTreeImplTestBase impl;
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());

  scoped_refptr<gpu::ClientSharedImage> shared_image =
      gpu::ClientSharedImage::CreateForTesting();

  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::WrapSharedImage(media::PIXEL_FORMAT_I420, shared_image,
                                         gpu::SyncToken(), base::DoNothing(),
                                         gfx::Size(10, 10), gfx::Rect(10, 10),
                                         gfx::Size(10, 10), base::TimeDelta());
  ASSERT_TRUE(video_frame);
  video_frame->metadata().allow_overlay = true;
  video_frame->set_shared_image_format_type(
      media::SharedImageFormatType::kSharedImageFormat);
  FakeVideoFrameProvider provider;
  provider.set_frame(video_frame);

  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_0);
  video_layer_impl->SetBounds(layer_size);
  video_layer_impl->SetDrawsContent(true);
  video_layer_impl->set_visible_layer_rect(gfx::Rect(layer_size));
  CopyProperties(impl.root_layer(), video_layer_impl);
  impl.CalcDrawProps(layer_size);

  gfx::Rect occluded;
  impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

  EXPECT_EQ(1u, impl.quad_list().size());
  const viz::DrawQuad* draw_quad = impl.quad_list().ElementAt(0);

  ASSERT_EQ(viz::DrawQuad::Material::kTextureContent, draw_quad->material);
  const auto* texture_draw_quad =
      static_cast<const viz::TextureDrawQuad*>(draw_quad);
  EXPECT_TRUE(texture_draw_quad->is_video_frame);
}

TEST(VideoLayerImplTest, NativeARGBFrameGeneratesTextureQuad) {
  gfx::Size layer_size(1000, 1000);

  LayerTreeImplTestBase impl;
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());

  scoped_refptr<gpu::ClientSharedImage> shared_image =
      gpu::ClientSharedImage::CreateForTesting();

  gfx::Size resource_size = gfx::Size(10, 10);
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::WrapSharedImage(media::PIXEL_FORMAT_ARGB, shared_image,
                                         gpu::SyncToken(), base::DoNothing(),
                                         resource_size, gfx::Rect(10, 10),
                                         resource_size, base::TimeDelta());
  ASSERT_TRUE(video_frame);
  video_frame->metadata().allow_overlay = true;
  FakeVideoFrameProvider provider;
  provider.set_frame(video_frame);

  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_0);
  video_layer_impl->SetBounds(layer_size);
  video_layer_impl->SetDrawsContent(true);
  video_layer_impl->set_visible_layer_rect(gfx::Rect(layer_size));
  CopyProperties(impl.root_layer(), video_layer_impl);

  impl.CalcDrawProps(layer_size);

  gfx::Rect occluded;
  impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

  EXPECT_EQ(1u, impl.quad_list().size());
  const viz::DrawQuad* draw_quad = impl.quad_list().ElementAt(0);
  ASSERT_EQ(viz::DrawQuad::Material::kTextureContent, draw_quad->material);

  const viz::TextureDrawQuad* texture_draw_quad =
      viz::TextureDrawQuad::MaterialCast(draw_quad);
  EXPECT_EQ(texture_draw_quad->resource_size_in_pixels(), resource_size);
}

TEST(VideoLayerImplTest, GetDamageReasons) {
  gfx::Size layer_size(1000, 1000);

  LayerTreeImplTestBase impl;
  DebugSetImplThreadAndMainThreadBlocked(impl.task_runner_provider());

  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, gfx::Size(10, 10), gfx::Rect(10, 10),
      gfx::Size(10, 10), base::TimeDelta());
  FakeVideoFrameProvider provider;
  provider.set_frame(video_frame);

  VideoLayerImpl* video_layer_impl = impl.AddLayerInActiveTree<VideoLayerImpl>(
      &provider, media::VIDEO_ROTATION_0);

  video_layer_impl->layer_tree_impl()->ResetAllChangeTracking();
  EXPECT_TRUE(video_layer_impl->GetDamageReasons().empty());
  video_layer_impl->SetBounds(layer_size);
  EXPECT_EQ(video_layer_impl->GetDamageReasons(),
            DamageReasonSet{DamageReason::kUntracked});

  video_layer_impl->layer_tree_impl()->ResetAllChangeTracking();
  EXPECT_TRUE(video_layer_impl->GetDamageReasons().empty());
  video_layer_impl->SetNeedsRedraw();
  EXPECT_EQ(video_layer_impl->GetDamageReasons(),
            DamageReasonSet{DamageReason::kVideoLayer});
}

}  // namespace
}  // namespace cc
