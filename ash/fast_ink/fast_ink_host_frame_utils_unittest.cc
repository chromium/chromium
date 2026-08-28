// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_host_frame_utils.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/math_util.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace ash {
namespace {

constexpr auto kTestContentRectInDIP = gfx::Rect(0, 0, 200, 100);
constexpr auto kTestTotalDamageRectInDIP = gfx::Rect(0, 0, 50, 25);

class FastInkHostCreateFrameUtilTestBase : public AshTestBase {
 public:
  FastInkHostCreateFrameUtilTestBase() = default;

  FastInkHostCreateFrameUtilTestBase(
      const FastInkHostCreateFrameUtilTestBase&) = delete;
  FastInkHostCreateFrameUtilTestBase& operator=(
      const FastInkHostCreateFrameUtilTestBase&) = delete;

  ~FastInkHostCreateFrameUtilTestBase() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("1000x500*2");

    auto* root_window = ash_test_helper()->GetHost()->window();
    gfx::Rect screen_bounds = root_window->GetBoundsInScreen();

    auto host_window =
        CreateWindowWithAppType(chromeos::AppType::NON_APP, screen_bounds,
                                kShellWindowId_OverlayContainer);

    // `host_window` is owned by the root_window and it will be deleted as
    // window hierarchy is deleted during Shell deletion.
    host_window_ = host_window.release();
    gfx::Size buffer_size = BufferSizeForHostWindow(host_window_.get());

    gpu::SharedImageUsageSet usage =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
        gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;
    if (shared_image_interface()
            ->GetCapabilities()
            .supports_scanout_shared_images) {
      usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    }

    shared_image_ = fast_ink_internal::CreateMappableSharedImage(
        buffer_size, usage, gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);
    ASSERT_TRUE(shared_image_);
  }

  // AshTestBase:
  void TearDown() override {
    shared_image_.reset();
    host_window_ = nullptr;
    client_resource_provider_.ShutdownAndReleaseAllResources();
    AshTestBase::TearDown();
  }

 protected:
  const gfx::Size BufferSizeForHostWindow(aura::Window* host_window) {
    const gfx::Transform& window_to_buffer_transform =
        host_window->GetHost()->GetRootTransform();
    gfx::Rect bounds(host_window->GetBoundsInScreen().size());

    return cc::MathUtil::MapEnclosingClippedRect(window_to_buffer_transform,
                                                 bounds)
        .size();
  }

  gpu::SharedImageInterface* shared_image_interface() {
    return fast_ink_internal::GetContextProvider()->SharedImageInterface();
  }

  viz::ClientResourceProvider client_resource_provider_;
  raw_ptr<aura::Window> host_window_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
};

using FastInkHostCreateFrameUtilTest = FastInkHostCreateFrameUtilTestBase;

TEST_F(FastInkHostCreateFrameUtilTest, HasValidSourceId) {
  auto frame = fast_ink_internal::CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRectInDIP,
      kTestTotalDamageRectInDIP, /*auto_update=*/true, *host_window_,
      client_resource_provider_, shared_image_, gpu::SyncToken());

  ASSERT_EQ(frame->resource_list.size(), 1u);
  viz::ResourceId resource_id = frame->resource_list.back().id;

  EXPECT_NE(resource_id, viz::kInvalidResourceId);
}

TEST_F(FastInkHostCreateFrameUtilTest, ResourceUsesMailbox) {
  auto frame = fast_ink_internal::CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRectInDIP,
      kTestTotalDamageRectInDIP, /*auto_update=*/true, *host_window_,
      client_resource_provider_, shared_image_, gpu::SyncToken());

  ASSERT_EQ(frame->resource_list.size(), 1u);
  EXPECT_EQ(frame->resource_list.back().mailbox(), shared_image_->mailbox());
}

TEST_F(FastInkHostCreateFrameUtilTest, CompositorFrameHasCorrectStructure) {
  auto frame = fast_ink_internal::CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRectInDIP,
      kTestTotalDamageRectInDIP, /*auto_update=*/true, *host_window_,
      client_resource_provider_, shared_image_, gpu::SyncToken());

  auto primary_display = display::Screen::Get()->GetPrimaryDisplay();

  // We should only have the root render pass.
  ASSERT_EQ(frame->render_pass_list.size(), 1u);

  // Frame size should be the size of `host_window_` in pixels. `host_window_`
  // is equal to the work area of the `primary_display`.
  EXPECT_EQ(frame->size_in_pixels(), gfx::Size(1000, 404));

  // We should have a single resource.
  EXPECT_EQ(frame->resource_list.size(), 1u);
  EXPECT_EQ(client_resource_provider_.num_resources_for_testing(), 1u);

  auto& render_pass = frame->render_pass_list.front();
  EXPECT_EQ(render_pass->quad_list.size(), 1u);
  EXPECT_EQ(render_pass->shared_quad_state_list.size(), 1u);

  EXPECT_EQ(frame->device_scale_factor(),
            primary_display.device_scale_factor());
}

TEST_F(FastInkHostCreateFrameUtilTest, FrameDamage_AutoModeOff) {
  auto frame = fast_ink_internal::CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRectInDIP,
      kTestTotalDamageRectInDIP, /*auto_update=*/false, *host_window_,
      client_resource_provider_, shared_image_, gpu::SyncToken());

  EXPECT_EQ(frame->render_pass_list.front()->damage_rect,
            gfx::Rect(0, 0, 100, 50));

  // If total damage is more than content_rect, we crop the damage to
  // the size of surface (size of `host_window_`).
  frame = fast_ink_internal::CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRectInDIP,
      gfx::Rect(0, 0, 501, 100), /*auto_update=*/false, *host_window_,
      client_resource_provider_, shared_image_, gpu::SyncToken());

  EXPECT_EQ(frame->render_pass_list.front()->damage_rect,
            gfx::Rect(0, 0, 1000, 200));
}

TEST_F(FastInkHostCreateFrameUtilTest, FrameDamage_AutoModeOn) {
  auto frame = fast_ink_internal::CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRectInDIP,
      kTestTotalDamageRectInDIP, /*auto_update=*/true, *host_window_,
      client_resource_provider_, shared_image_, gpu::SyncToken());

  // In auto update mode, we damage the full output rect, regardless of the
  // specified total_damage_rect.
  EXPECT_EQ(frame->render_pass_list.front()->damage_rect,
            gfx::Rect(frame->size_in_pixels()));
}

TEST_F(FastInkHostCreateFrameUtilTest, LowPriorityHintKillswitch) {
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        features::kFastInkHostLowPriorityHint);

    auto frame_no_auto = fast_ink_internal::CreateCompositorFrame(
        viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRectInDIP,
        kTestTotalDamageRectInDIP, /*auto_update=*/false, *host_window_,
        client_resource_provider_, shared_image_, gpu::SyncToken());

    EXPECT_FALSE(frame_no_auto->resource_list.back().GetIsOverlayCandidate());
    auto* texture_quad_no_auto = viz::TextureDrawQuad::MaterialCast(
        frame_no_auto->render_pass_list.front()->quad_list.back());
    EXPECT_EQ(texture_quad_no_auto->overlay_priority_hint,
              viz::OverlayPriority::kRegular);

    auto frame_auto = fast_ink_internal::CreateCompositorFrame(
        viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRectInDIP,
        kTestTotalDamageRectInDIP, /*auto_update=*/true, *host_window_,
        client_resource_provider_, shared_image_, gpu::SyncToken());

    EXPECT_TRUE(frame_auto->resource_list.back().GetIsOverlayCandidate());
    auto* texture_quad_auto = viz::TextureDrawQuad::MaterialCast(
        frame_auto->render_pass_list.front()->quad_list.back());
    EXPECT_EQ(texture_quad_auto->overlay_priority_hint,
              viz::OverlayPriority::kRegular);
  }

  client_resource_provider_.ShutdownAndReleaseAllResources();

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        features::kFastInkHostLowPriorityHint);

    auto frame_no_auto = fast_ink_internal::CreateCompositorFrame(
        viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRectInDIP,
        kTestTotalDamageRectInDIP, /*auto_update=*/false, *host_window_,
        client_resource_provider_, shared_image_, gpu::SyncToken());

    auto* texture_quad_no_auto = viz::TextureDrawQuad::MaterialCast(
        frame_no_auto->render_pass_list.front()->quad_list.back());
    EXPECT_EQ(texture_quad_no_auto->overlay_priority_hint,
              viz::OverlayPriority::kNone);

    auto frame_auto = fast_ink_internal::CreateCompositorFrame(
        viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRectInDIP,
        kTestTotalDamageRectInDIP, /*auto_update=*/true, *host_window_,
        client_resource_provider_, shared_image_, gpu::SyncToken());

    auto* texture_quad_auto = viz::TextureDrawQuad::MaterialCast(
        frame_auto->render_pass_list.front()->quad_list.back());
    EXPECT_EQ(texture_quad_auto->overlay_priority_hint,
              viz::OverlayPriority::kRegular);
  }
}

}  // namespace
}  // namespace ash
