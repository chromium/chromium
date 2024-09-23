// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/test/render_pass_test_utils.h"

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/video_types.h"

namespace cc {

namespace {

viz::ResourceId CreateAndImportResource(
    viz::ClientResourceProvider* resource_provider,
    const gpu::SyncToken& sync_token,
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB()) {
  constexpr gfx::Size size(64, 64);
  auto transfer_resource = viz::TransferableResource::MakeGpu(
      gpu::Mailbox::Generate(), GL_TEXTURE_2D, sync_token, size,
      viz::SinglePlaneFormat::kRGBA_8888, false /* is_overlay_candidate */);
  transfer_resource.color_space = std::move(color_space);
  return resource_provider->ImportResource(transfer_resource,
                                           base::DoNothing());
}

}  // anonymous namespace

viz::CompositorRenderPass* AddRenderPass(
    viz::CompositorRenderPassList* pass_list,
    viz::CompositorRenderPassId render_pass_id,
    const gfx::Rect& output_rect,
    const gfx::Transform& root_transform,
    const FilterOperations& filters) {
  auto pass = viz::CompositorRenderPass::Create();
  pass->SetNew(render_pass_id, output_rect, output_rect, root_transform);
  pass->filters = filters;
  viz::CompositorRenderPass* saved = pass.get();
  pass_list->push_back(std::move(pass));
  return saved;
}

viz::AggregatedRenderPass* AddRenderPass(
    viz::AggregatedRenderPassList* pass_list,
    viz::AggregatedRenderPassId render_pass_id,
    const gfx::Rect& output_rect,
    const gfx::Transform& root_transform,
    const FilterOperations& filters) {
  auto pass = std::make_unique<viz::AggregatedRenderPass>();
  pass->SetNew(render_pass_id, output_rect, output_rect, root_transform);
  pass->filters = filters;
  auto* saved = pass.get();
  pass_list->push_back(std::move(pass));
  return saved;
}

viz::CompositorRenderPass* AddRenderPassWithDamage(
    viz::CompositorRenderPassList* pass_list,
    viz::CompositorRenderPassId render_pass_id,
    const gfx::Rect& output_rect,
    const gfx::Rect& damage_rect,
    const gfx::Transform& root_transform,
    const FilterOperations& filters) {
  auto pass = viz::CompositorRenderPass::Create();
  pass->SetNew(render_pass_id, output_rect, damage_rect, root_transform);
  pass->filters = filters;
  viz::CompositorRenderPass* saved = pass.get();
  pass_list->push_back(std::move(pass));
  return saved;
}

viz::AggregatedRenderPass* AddRenderPassWithDamage(
    viz::AggregatedRenderPassList* pass_list,
    viz::AggregatedRenderPassId render_pass_id,
    const gfx::Rect& output_rect,
    const gfx::Rect& damage_rect,
    const gfx::Transform& root_transform,
    const FilterOperations& filters) {
  auto pass = std::make_unique<viz::AggregatedRenderPass>();
  pass->SetNew(render_pass_id, output_rect, damage_rect, root_transform);
  pass->filters = filters;
  auto* saved = pass.get();
  pass_list->push_back(std::move(pass));
  return saved;
}

viz::SolidColorDrawQuad* AddClippedQuad(viz::AggregatedRenderPass* pass,
                                        const gfx::Rect& rect,
                                        SkColor4f color) {
  viz::SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, gfx::MaskFilterInfo(),
                       rect, /*contents_opaque=*/false, /*opacity_f=*/1,
                       SkBlendMode::kSrcOver, /*sorting_context=*/0,
                       /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  auto* quad = pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

viz::SolidColorDrawQuad* AddTransformedQuad(viz::AggregatedRenderPass* pass,
                                            const gfx::Rect& rect,
                                            SkColor4f color,
                                            const gfx::Transform& transform) {
  viz::SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform, rect, rect, gfx::MaskFilterInfo(),
                       /*clip=*/std::nullopt, /*contents_opaque=*/false,
                       /*opacity_f=*/1, SkBlendMode::kSrcOver,
                       /*sorting_context=*/0, /*layer_id=*/0u,
                       /*fast_rounded_corner=*/false);
  auto* quad = pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

template <typename QuadType, typename RenderPassType>
QuadType* AddRenderPassQuadInternal(RenderPassType* to_pass,
                                    RenderPassType* contributing_pass) {
  gfx::Rect output_rect = contributing_pass->output_rect;
  viz::SharedQuadState* shared_state =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), output_rect, output_rect,
                       gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                       /*contents_opaque=*/false, /*opacity_f=*/1,
                       SkBlendMode::kSrcOver, /*sorting_context=*/0,
                       /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  auto* quad = to_pass->template CreateAndAppendDrawQuad<QuadType>();
  quad->SetNew(shared_state, output_rect, output_rect, contributing_pass->id,
               viz::kInvalidResourceId, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1.0f, 1.0f), gfx::PointF(), gfx::RectF(), false,
               1.0f);
  return quad;
}

viz::CompositorRenderPassDrawQuad* AddRenderPassQuad(
    viz::CompositorRenderPass* to_pass,
    viz::CompositorRenderPass* contributing_pass) {
  return AddRenderPassQuadInternal<viz::CompositorRenderPassDrawQuad>(
      to_pass, contributing_pass);
}
viz::AggregatedRenderPassDrawQuad* AddRenderPassQuad(
    viz::AggregatedRenderPass* to_pass,
    viz::AggregatedRenderPass* contributing_pass) {
  return AddRenderPassQuadInternal<viz::AggregatedRenderPassDrawQuad>(
      to_pass, contributing_pass);
}

void AddRenderPassQuad(viz::AggregatedRenderPass* to_pass,
                       viz::AggregatedRenderPass* contributing_pass,
                       viz::ResourceId mask_resource_id,
                       gfx::Transform transform,
                       SkBlendMode blend_mode) {
  gfx::Rect output_rect = contributing_pass->output_rect;
  viz::SharedQuadState* shared_state =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(
      transform, output_rect, output_rect, gfx::MaskFilterInfo(),
      /*clip=*/std::nullopt, /*contents_opaque=*/false, 1, blend_mode,
      /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  auto* quad =
      to_pass->CreateAndAppendDrawQuad<viz::AggregatedRenderPassDrawQuad>();
  gfx::Size arbitrary_nonzero_size(1, 1);
  quad->SetNew(shared_state, output_rect, output_rect, contributing_pass->id,
               mask_resource_id, gfx::RectF(output_rect),
               arbitrary_nonzero_size, gfx::Vector2dF(), gfx::PointF(),
               gfx::RectF(), false, 1.0f);
}

std::vector<viz::ResourceId> AddOneOfEveryQuadType(
    viz::CompositorRenderPass* to_pass,
    viz::ClientResourceProvider* resource_provider,
    viz::CompositorRenderPassId child_pass_id) {
  gfx::Rect rect(0, 0, 100, 100);
  gfx::Rect visible_rect(0, 0, 100, 100);
  bool needs_blending = true;

  static const gpu::SyncToken kSyncToken(
      gpu::CommandBufferNamespace::GPU_IO,
      gpu::CommandBufferId::FromUnsafeValue(0x123), 30);

  viz::ResourceId resource1 =
      CreateAndImportResource(resource_provider, kSyncToken);
  viz::ResourceId resource2 =
      CreateAndImportResource(resource_provider, kSyncToken);
  viz::ResourceId resource3 =
      CreateAndImportResource(resource_provider, kSyncToken);
  viz::ResourceId resource4 =
      CreateAndImportResource(resource_provider, kSyncToken);
  viz::ResourceId resource5 =
      CreateAndImportResource(resource_provider, kSyncToken);
  viz::ResourceId resource6 =
      CreateAndImportResource(resource_provider, kSyncToken);
  viz::ResourceId resource8 =
      CreateAndImportResource(resource_provider, kSyncToken);

  viz::SharedQuadState* shared_state =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, gfx::MaskFilterInfo(),
                       /*clip=*/std::nullopt, /*contents_opaque=*/false,
                       /*opacity_f=*/1, SkBlendMode::kSrcOver,
                       /*sorting_context=*/0, /*layer_id=*/0u,
                       /*fast_rounded_corner=*/false);

  auto* debug_border_quad =
      to_pass->CreateAndAppendDrawQuad<viz::DebugBorderDrawQuad>();
  debug_border_quad->SetNew(shared_state, rect, visible_rect, SkColors::kRed,
                            1);

  if (child_pass_id) {
    auto* render_pass_quad =
        to_pass->CreateAndAppendDrawQuad<viz::CompositorRenderPassDrawQuad>();
    render_pass_quad->SetNew(shared_state, rect, visible_rect, child_pass_id,
                             resource5, gfx::RectF(rect), gfx::Size(73, 26),
                             gfx::Vector2dF(1.0f, 1.0f), gfx::PointF(),
                             gfx::RectF(), false, 1.0f);
  }

  auto* solid_color_quad =
      to_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  solid_color_quad->SetNew(shared_state, rect, visible_rect, SkColors::kRed,
                           false);

  // We add a TextureDrawQuad with is_stream_video set to true to cover related
  // code paths.
  auto* stream_video_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  stream_video_quad->SetNew(
      shared_state, rect, visible_rect, needs_blending, resource6, false,
      gfx::PointF(0.f, 0.f), gfx::PointF(1.f, 1.f), SkColors::kTransparent,
      false, false, false, gfx::ProtectedVideoType::kHardwareProtected);
  stream_video_quad->is_stream_video = true;

  auto* texture_quad = to_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  texture_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                       resource1, false, gfx::PointF(0.f, 0.f),
                       gfx::PointF(1.f, 1.f), SkColors::kTransparent, false,
                       false, false, gfx::ProtectedVideoType::kClear);

  auto* external_resource_texture_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  external_resource_texture_quad->SetNew(
      shared_state, rect, visible_rect, needs_blending, resource8, false,
      gfx::PointF(0.f, 0.f), gfx::PointF(1.f, 1.f), SkColors::kTransparent,
      false, false, false, gfx::ProtectedVideoType::kClear);

  auto* scaled_tile_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  scaled_tile_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                           resource2, gfx::RectF(0, 0, 50, 50),
                           gfx::Size(50, 50), false, false, false);

  viz::SharedQuadState* transformed_state =
      to_pass->CreateAndAppendSharedQuadState();
  *transformed_state = *shared_state;
  gfx::Transform rotation;
  rotation.Rotate(45);
  transformed_state->quad_to_target_transform =
      transformed_state->quad_to_target_transform * rotation;
  auto* transformed_tile_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  transformed_tile_quad->SetNew(
      transformed_state, rect, visible_rect, needs_blending, resource3,
      gfx::RectF(0, 0, 100, 100), gfx::Size(100, 100), false, false, false);

  viz::SharedQuadState* shared_state2 =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, gfx::MaskFilterInfo(),
                       /*clip=*/std::nullopt, /*contents_opaque=*/false,
                       /*opacity_f=*/1, SkBlendMode::kSrcOver,
                       /*sorting_context=*/0, /*layer_id=*/0u,
                       /*fast_rounded_corner=*/false);

  auto* tile_quad = to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  tile_quad->SetNew(shared_state2, rect, visible_rect, needs_blending,
                    resource4, gfx::RectF(0, 0, 100, 100), gfx::Size(100, 100),
                    false, false, false);

  return {resource1, resource2, resource3, resource4,
          resource5, resource6, resource8};
}

static void CollectResources(std::vector<viz::ReturnedResource>* array,
                             std::vector<viz::ReturnedResource> returned) {}

void AddOneOfEveryQuadTypeInDisplayResourceProvider(
    viz::AggregatedRenderPass* to_pass,
    viz::DisplayResourceProvider* resource_provider,
    viz::ClientResourceProvider* child_resource_provider,
    viz::RasterContextProvider* child_context_provider,
    viz::AggregatedRenderPassId child_pass_id,
    gpu::SyncToken* sync_token_for_mailbox_tebxture) {
  gfx::Rect rect(0, 0, 100, 100);
  gfx::Rect visible_rect(0, 0, 100, 100);
  bool needs_blending = true;
  static const gpu::SyncToken kDefaultSyncToken(
      gpu::CommandBufferNamespace::GPU_IO,
      gpu::CommandBufferId::FromUnsafeValue(0x111), 42);
  static const gpu::SyncToken kSyncTokenForMailboxTextureQuad(
      gpu::CommandBufferNamespace::GPU_IO,
      gpu::CommandBufferId::FromUnsafeValue(0x123), 30);
  *sync_token_for_mailbox_tebxture = kSyncTokenForMailboxTextureQuad;

  viz::ResourceId resource1 =
      CreateAndImportResource(child_resource_provider, kDefaultSyncToken);
  viz::ResourceId resource2 =
      CreateAndImportResource(child_resource_provider, kDefaultSyncToken);
  viz::ResourceId resource3 =
      CreateAndImportResource(child_resource_provider, kDefaultSyncToken);
  viz::ResourceId resource4 =
      CreateAndImportResource(child_resource_provider, kDefaultSyncToken);
  viz::ResourceId resource5 =
      CreateAndImportResource(child_resource_provider, kDefaultSyncToken);
  viz::ResourceId resource6 =
      CreateAndImportResource(child_resource_provider, kDefaultSyncToken);
  viz::ResourceId resource7 =
      CreateAndImportResource(child_resource_provider, kDefaultSyncToken);
  viz::ResourceId resource8 = CreateAndImportResource(
      child_resource_provider, kSyncTokenForMailboxTextureQuad);

  // Transfer resource to the parent.
  std::vector<viz::ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource1);
  resource_ids_to_transfer.push_back(resource2);
  resource_ids_to_transfer.push_back(resource3);
  resource_ids_to_transfer.push_back(resource4);
  resource_ids_to_transfer.push_back(resource5);
  resource_ids_to_transfer.push_back(resource6);
  resource_ids_to_transfer.push_back(resource7);
  resource_ids_to_transfer.push_back(resource8);

  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id = resource_provider->CreateChild(
      base::BindRepeating(&CollectResources, &returned_to_child),
      viz::SurfaceId());

  // Transfer resource to the parent.
  std::vector<viz::TransferableResource> list;
  child_resource_provider->PrepareSendToParent(resource_ids_to_transfer, &list,
                                               child_context_provider);
  resource_provider->ReceiveFromChild(child_id, list);

  // Delete them in the child so they won't be leaked, and will be released once
  // returned from the parent. This assumes they won't need to be sent to the
  // parent again.
  for (viz::ResourceId id : resource_ids_to_transfer)
    child_resource_provider->RemoveImportedResource(id);

  // Before create DrawQuad in viz::DisplayResourceProvider's namespace, get the
  // mapped resource id first.
  std::unordered_map<viz::ResourceId, viz::ResourceId, viz::ResourceIdHasher>
      resource_map = resource_provider->GetChildToParentMap(child_id);
  viz::ResourceId mapped_resource1 = resource_map[resource1];
  viz::ResourceId mapped_resource2 = resource_map[resource2];
  viz::ResourceId mapped_resource3 = resource_map[resource3];
  viz::ResourceId mapped_resource4 = resource_map[resource4];
  viz::ResourceId mapped_resource5 = resource_map[resource5];
  viz::ResourceId mapped_resource6 = resource_map[resource6];
  viz::ResourceId mapped_resource8 = resource_map[resource8];

  viz::SharedQuadState* shared_state =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, gfx::MaskFilterInfo(),
                       /*clip=*/std::nullopt, /*contents_opaque=*/false,
                       /*opacity_f=*/1, SkBlendMode::kSrcOver,
                       /*sorting_context=*/0, /*layer_id=*/0u,
                       /*fast_rounded_corner=*/false);

  viz::DebugBorderDrawQuad* debug_border_quad =
      to_pass->CreateAndAppendDrawQuad<viz::DebugBorderDrawQuad>();
  debug_border_quad->SetNew(shared_state, rect, visible_rect, SkColors::kRed,
                            1);
  if (child_pass_id) {
    auto* render_pass_quad =
        to_pass->CreateAndAppendDrawQuad<viz::AggregatedRenderPassDrawQuad>();
    render_pass_quad->SetNew(shared_state, rect, visible_rect, child_pass_id,
                             mapped_resource5, gfx::RectF(rect),
                             gfx::Size(73, 26), gfx::Vector2dF(), gfx::PointF(),
                             gfx::RectF(), false, 1.0f);
  }

  viz::SolidColorDrawQuad* solid_color_quad =
      to_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  solid_color_quad->SetNew(shared_state, rect, visible_rect, SkColors::kRed,
                           false);

  viz::TextureDrawQuad* stream_video_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  stream_video_quad->SetNew(
      shared_state, rect, visible_rect, needs_blending, mapped_resource6, false,
      gfx::PointF(0.f, 0.f), gfx::PointF(1.f, 1.f), SkColors::kTransparent,
      false, false, false, gfx::ProtectedVideoType::kHardwareProtected);
  stream_video_quad->is_stream_video = true;

  viz::TextureDrawQuad* texture_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  texture_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                       mapped_resource1, false, gfx::PointF(0.f, 0.f),
                       gfx::PointF(1.f, 1.f), SkColors::kTransparent, false,
                       false, false, gfx::ProtectedVideoType::kClear);

  viz::TextureDrawQuad* external_resource_texture_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  external_resource_texture_quad->SetNew(
      shared_state, rect, visible_rect, needs_blending, mapped_resource8, false,
      gfx::PointF(0.f, 0.f), gfx::PointF(1.f, 1.f), SkColors::kTransparent,
      false, false, false, gfx::ProtectedVideoType::kClear);

  viz::TileDrawQuad* scaled_tile_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  scaled_tile_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                           mapped_resource2, gfx::RectF(0, 0, 50, 50),
                           gfx::Size(50, 50), false, false, false);

  viz::SharedQuadState* transformed_state =
      to_pass->CreateAndAppendSharedQuadState();
  *transformed_state = *shared_state;
  gfx::Transform rotation;
  rotation.Rotate(45);
  transformed_state->quad_to_target_transform =
      transformed_state->quad_to_target_transform * rotation;
  viz::TileDrawQuad* transformed_tile_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  transformed_tile_quad->SetNew(
      transformed_state, rect, visible_rect, needs_blending, mapped_resource3,
      gfx::RectF(0, 0, 100, 100), gfx::Size(100, 100), false, false, false);

  viz::SharedQuadState* shared_state2 =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state2->SetAll(gfx::Transform(), rect, rect, gfx::MaskFilterInfo(),
                        /*clip=*/std::nullopt, /*contents_opaque=*/false,
                        /*opacity_f=*/1, SkBlendMode::kSrcOver,
                        /*sorting_context=*/0, /*layer_id=*/0u,
                        /*fast_rounded_corner=*/false);

  viz::TileDrawQuad* tile_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  tile_quad->SetNew(shared_state2, rect, visible_rect, needs_blending,
                    mapped_resource4, gfx::RectF(0, 0, 100, 100),
                    gfx::Size(100, 100), false, false, false);
}

std::unique_ptr<viz::AggregatedRenderPass> CopyToAggregatedRenderPass(
    viz::CompositorRenderPass* from_pass,
    viz::AggregatedRenderPassId to_id,
    gfx::ContentColorUsage content_usage) {
  auto copy_pass = std::make_unique<viz::AggregatedRenderPass>(
      from_pass->shared_quad_state_list.size(), from_pass->quad_list.size());
  copy_pass->SetAll(to_id, from_pass->output_rect, from_pass->damage_rect,
                    from_pass->transform_to_root_target, from_pass->filters,
                    from_pass->backdrop_filters,
                    from_pass->backdrop_filter_bounds, content_usage,
                    from_pass->has_transparent_background,
                    from_pass->cache_render_pass,
                    from_pass->has_damage_from_contributing_content,
                    from_pass->generate_mipmap);

  copy_pass->shared_quad_state_list =
      std::move(from_pass->shared_quad_state_list);
  copy_pass->quad_list = std::move(from_pass->quad_list);

  return copy_pass;
}

}  // namespace cc
