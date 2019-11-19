// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include "base/auto_reset.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/layers/recording_source.h"
#include "cc/paint/paint_record.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/transform_node.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

PictureLayer::PictureLayerInputs::PictureLayerInputs() = default;

PictureLayer::PictureLayerInputs::~PictureLayerInputs() = default;

scoped_refptr<PictureLayer> PictureLayer::Create(ContentLayerClient* client) {
  return base::WrapRefCounted(new PictureLayer(client));
}

PictureLayer::PictureLayer(ContentLayerClient* client)
    : instrumentation_object_tracker_(id()), update_source_frame_number_(-1) {
  picture_layer_inputs_.client = client;
}

PictureLayer::PictureLayer(ContentLayerClient* client,
                           std::unique_ptr<RecordingSource> source)
    : PictureLayer(client) {
  recording_source_ = std::move(source);
}

PictureLayer::~PictureLayer() = default;

std::unique_ptr<LayerImpl> PictureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id());
}

void PictureLayer::PushPropertiesTo(LayerImpl* base_layer) {
  // TODO(enne): http://crbug.com/918126 debugging
  CHECK(this);

  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  Layer::PushPropertiesTo(base_layer);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "PictureLayer::PushPropertiesTo");
  DropRecordingSourceContentIfInvalid();

  layer_impl->SetNearestNeighbor(picture_layer_inputs_.nearest_neighbor);
  layer_impl->SetUseTransformedRasterization(
      ShouldUseTransformedRasterization());
  layer_impl->set_gpu_raster_max_texture_size(
      layer_tree_host()->device_viewport_rect().size());
  layer_impl->SetIsBackdropFilterMask(is_backdrop_filter_mask());

  // TODO(enne): http://crbug.com/918126 debugging
  CHECK(this);
  if (!recording_source_) {
    bool valid_host = layer_tree_host();
    bool has_parent = parent();
    bool parent_has_host = parent() && parent()->layer_tree_host();

    auto str = base::StringPrintf("vh: %d, hp: %d, phh: %d", valid_host,
                                  has_parent, parent_has_host);
    static auto* crash_key = base::debug::AllocateCrashKeyString(
        "issue918126", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(crash_key, str);
    base::debug::DumpWithoutCrashing();
  }

  layer_impl->UpdateRasterSource(recording_source_->CreateRasterSource(),
                                 &last_updated_invalidation_, nullptr, nullptr);
  DCHECK(last_updated_invalidation_.IsEmpty());
}

void PictureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  Layer::SetLayerTreeHost(host);

  if (!host)
    return;

  if (!recording_source_)
    recording_source_.reset(new RecordingSource);
  recording_source_->SetSlowdownRasterScaleFactor(
      host->GetDebugState().slow_down_raster_scale_factor);

  // Source frame numbers are relative the LayerTreeHost, so this needs
  // to be reset.
  update_source_frame_number_ = -1;
}

void PictureLayer::SetNeedsDisplayRect(const gfx::Rect& layer_rect) {
  DCHECK(!layer_tree_host() || !layer_tree_host()->in_paint_layer_contents());
  if (recording_source_)
    recording_source_->SetNeedsDisplayRect(layer_rect);
  Layer::SetNeedsDisplayRect(layer_rect);
}

bool PictureLayer::Update() {
  update_source_frame_number_ = layer_tree_host()->SourceFrameNumber();
  bool updated = Layer::Update();

  gfx::Size layer_size = bounds();

  recording_source_->SetBackgroundColor(SafeOpaqueBackgroundColor());
  recording_source_->SetRequiresClear(
      !contents_opaque() &&
      !picture_layer_inputs_.client->FillsBoundsCompletely());

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"), "PictureLayer::Update",
               "source_frame_number", layer_tree_host()->SourceFrameNumber());
  devtools_instrumentation::ScopedLayerTreeTask update_layer(
      devtools_instrumentation::kUpdateLayer, id(), layer_tree_host()->GetId());

  // UpdateAndExpandInvalidation will give us an invalidation that covers
  // anything not explicitly recorded in this frame. We give this region
  // to the impl side so that it drops tiles that may not have a recording
  // for them.
  DCHECK(picture_layer_inputs_.client);

  picture_layer_inputs_.recorded_viewport =
      picture_layer_inputs_.client->PaintableRegion();

  updated |= recording_source_->UpdateAndExpandInvalidation(
      &last_updated_invalidation_, layer_size,
      picture_layer_inputs_.recorded_viewport);

  if (updated) {
    picture_layer_inputs_.display_list =
        picture_layer_inputs_.client->PaintContentsToDisplayList(
            ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
    picture_layer_inputs_.painter_reported_memory_usage =
        picture_layer_inputs_.client->GetApproximateUnsharedMemoryUsage();
    recording_source_->UpdateDisplayItemList(
        picture_layer_inputs_.display_list,
        picture_layer_inputs_.painter_reported_memory_usage,
        layer_tree_host()->recording_scale_factor());

    SetNeedsPushProperties();
    IncreasePaintCount();
  } else {
    // If this invalidation did not affect the recording source, then it can be
    // cleared as an optimization.
    last_updated_invalidation_.Clear();
  }

  return updated;
}

sk_sp<SkPicture> PictureLayer::GetPicture() const {
  // We could either flatten the RecordingSource into a single SkPicture, or
  // paint a fresh one depending on what we intend to do with it.  For now we
  // just paint a fresh one to get consistent results.
  if (!DrawsContent())
    return nullptr;

  gfx::Size layer_size = bounds();
  RecordingSource recording_source;
  Region recording_invalidation;

  gfx::Rect new_recorded_viewport =
      picture_layer_inputs_.client->PaintableRegion();
  scoped_refptr<DisplayItemList> display_list =
      picture_layer_inputs_.client->PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  size_t painter_reported_memory_usage =
      picture_layer_inputs_.client->GetApproximateUnsharedMemoryUsage();

  recording_source.UpdateAndExpandInvalidation(
      &recording_invalidation, layer_size, new_recorded_viewport);
  recording_source.UpdateDisplayItemList(
      display_list, painter_reported_memory_usage,
      layer_tree_host()->recording_scale_factor());

  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();
  return raster_source->GetFlattenedPicture();
}

void PictureLayer::ClearClient() {
  picture_layer_inputs_.client = nullptr;
  UpdateDrawsContent(HasDrawableContent());
}

void PictureLayer::SetNearestNeighbor(bool nearest_neighbor) {
  if (picture_layer_inputs_.nearest_neighbor == nearest_neighbor)
    return;

  picture_layer_inputs_.nearest_neighbor = nearest_neighbor;
  SetNeedsCommit();
}

void PictureLayer::SetTransformedRasterizationAllowed(bool allowed) {
  if (picture_layer_inputs_.transformed_rasterization_allowed == allowed)
    return;

  picture_layer_inputs_.transformed_rasterization_allowed = allowed;
  SetNeedsCommit();
}

bool PictureLayer::HasDrawableContent() const {
  return picture_layer_inputs_.client && Layer::HasDrawableContent();
}

void PictureLayer::SetIsBackdropFilterMask(bool is_backdrop_filter_mask) {
  if (picture_layer_inputs_.is_backdrop_filter_mask == is_backdrop_filter_mask)
    return;

  picture_layer_inputs_.is_backdrop_filter_mask = is_backdrop_filter_mask;
  SetNeedsCommit();
}

void PictureLayer::RunMicroBenchmark(MicroBenchmark* benchmark) {
  benchmark->RunOnLayer(this);
}

void PictureLayer::CaptureContent(const gfx::Rect& rect,
                                  std::vector<NodeId>* content) {
  if (!DrawsContent())
    return;

  const DisplayItemList* display_item_list = GetDisplayItemList();
  if (!display_item_list)
    return;

  // We could run into this situation as CaptureContent could start at any time.
  if (transform_tree_index() == TransformTree::kInvalidNodeId)
    return;

  gfx::Transform inverse_screen_space_transform;
  if (!ScreenSpaceTransform().GetInverse(&inverse_screen_space_transform))
    return;
  gfx::Rect transformed = MathUtil::ProjectEnclosingClippedRect(
      inverse_screen_space_transform, rect);

  transformed.Intersect(gfx::Rect(bounds()));
  if (transformed.IsEmpty())
    return;

  display_item_list->CaptureContent(transformed, content);
}

void PictureLayer::DropRecordingSourceContentIfInvalid() {
  int source_frame_number = layer_tree_host()->SourceFrameNumber();
  gfx::Size recording_source_bounds = recording_source_->GetSize();

  gfx::Size layer_bounds = bounds();

  // If update called, then recording source size must match bounds pushed to
  // impl layer.
  DCHECK(update_source_frame_number_ != source_frame_number ||
         layer_bounds == recording_source_bounds)
      << " bounds " << layer_bounds.ToString() << " recording source "
      << recording_source_bounds.ToString();

  if (update_source_frame_number_ != source_frame_number &&
      recording_source_bounds != layer_bounds) {
    // Update may not get called for the layer (if it's not in the viewport
    // for example), even though it has resized making the recording source no
    // longer valid. In this case just destroy the recording source.
    recording_source_->SetEmptyBounds();
    picture_layer_inputs_.recorded_viewport = gfx::Rect();
    picture_layer_inputs_.display_list = nullptr;
    picture_layer_inputs_.painter_reported_memory_usage = 0;
  }
}

bool PictureLayer::ShouldUseTransformedRasterization() const {
  if (!picture_layer_inputs_.transformed_rasterization_allowed)
    return false;

  // Background color overfill is undesirable with transformed rasterization.
  // However, without background overfill, the tiles will be non-opaque on
  // external edges, and layer opaque region can't be computed in layer space
  // due to rounding under extreme scaling. This defeats many opaque layer
  // optimization. Prefer optimization over quality for this particular case.
  if (contents_opaque())
    return false;

  const TransformTree& transform_tree =
      layer_tree_host()->property_trees()->transform_tree;
  DCHECK(!transform_tree.needs_update());
  auto* transform_node = transform_tree.Node(transform_tree_index());
  DCHECK(transform_node);
  // TODO(pdr): This is a workaround for https://crbug.com/708951 to avoid
  // crashing when there's no transform node. This workaround should be removed.
  if (!transform_node)
    return false;

  if (transform_node->to_screen_is_potentially_animated)
    return false;

  const gfx::Transform& to_screen =
      transform_tree.ToScreen(transform_tree_index());
  if (!to_screen.IsScaleOrTranslation())
    return false;

  float origin_x =
      to_screen.matrix().getFloat(0, 3) + offset_to_transform_parent().x();
  float origin_y =
      to_screen.matrix().getFloat(1, 3) + offset_to_transform_parent().y();
  if (origin_x - floorf(origin_x) == 0.f && origin_y - floorf(origin_y) == 0.f)
    return false;

  return true;
}

const DisplayItemList* PictureLayer::GetDisplayItemList() {
  return picture_layer_inputs_.display_list.get();
}

}  // namespace cc
