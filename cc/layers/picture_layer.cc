// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/features.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/layers/recording_source.h"
#include "cc/paint/paint_record.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/transform_node.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
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
  recording_source_.Write(*this) = std::move(source);
}

PictureLayer::~PictureLayer() = default;

std::unique_ptr<LayerImpl> PictureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return PictureLayerImpl::Create(tree_impl, id());
}

void PictureLayer::PushPropertiesTo(
    LayerImpl* base_layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  // TODO(enne): http://crbug.com/918126 debugging
  CHECK(this);

  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  Layer::PushPropertiesTo(base_layer, commit_state, unsafe_state);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "PictureLayer::PushPropertiesTo");
  DropRecordingSourceContentIfInvalid(
      base_layer->layer_tree_impl()->source_frame_number());

  layer_impl->SetNearestNeighbor(picture_layer_inputs_.nearest_neighbor);
  layer_impl->set_gpu_raster_max_texture_size(
      commit_state.device_viewport_rect.size());
  layer_impl->SetIsBackdropFilterMask(is_backdrop_filter_mask());
  layer_impl->SetDirectlyCompositedImageDefaultRasterScale(
      picture_layer_inputs_.directly_composited_image_default_raster_scale);

  // TODO(enne): http://crbug.com/918126 debugging
  CHECK(this);
  if (!recording_source_.Read(*this)) {
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

  layer_impl->UpdateRasterSource(
      recording_source_.Read(*this)->CreateRasterSource(),
      &last_updated_invalidation_.Write(*this), nullptr, nullptr);
  DCHECK(last_updated_invalidation_.Read(*this).IsEmpty());
}

void PictureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  Layer::SetLayerTreeHost(host);

  if (!host)
    return;

  if (!recording_source_.Read(*this))
    recording_source_.Write(*this) = std::make_unique<RecordingSource>();
  recording_source_.Write(*this)->SetSlowdownRasterScaleFactor(
      host->GetDebugState().slow_down_raster_scale_factor);

  // Source frame numbers are relative the LayerTreeHost, so this needs
  // to be reset.
  update_source_frame_number_.Write(*this) = -1;
}

void PictureLayer::SetNeedsDisplayRect(const gfx::Rect& layer_rect) {
  DCHECK(IsPropertyChangeAllowed());
  if (recording_source_.Read(*this))
    recording_source_.Write(*this)->SetNeedsDisplayRect(layer_rect);
  Layer::SetNeedsDisplayRect(layer_rect);
}

bool PictureLayer::RequiresSetNeedsDisplayOnHdrHeadroomChange() const {
  const auto& display_list = picture_layer_inputs_.display_list;
  if (display_list &&
      display_list->discardable_image_map().content_color_usage() ==
          gfx::ContentColorUsage::kHDR) {
    return true;
  }
  return false;
}

bool PictureLayer::Update() {
  update_source_frame_number_.Write(*this) =
      layer_tree_host()->SourceFrameNumber();
  bool updated = Layer::Update();

  gfx::Size layer_size = bounds();

  auto& recording_source = recording_source_.Write(*this);
  recording_source->SetBackgroundColor(SafeOpaqueBackgroundColor());
  recording_source->SetRequiresClear(
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

  auto recorded_viewport = picture_layer_inputs_.client->PaintableRegion();

  updated |= recording_source->UpdateAndExpandInvalidation(
      &last_updated_invalidation_.Write(*this), layer_size, recorded_viewport);

  if (updated) {
    {
      auto old_display_list = std::move(picture_layer_inputs_.display_list);
      picture_layer_inputs_.display_list =
          picture_layer_inputs_.client->PaintContentsToDisplayList();
      if (old_display_list &&
          picture_layer_inputs_.display_list
              ->NeedsAdditionalInvalidationForLCDText(*old_display_list)) {
        last_updated_invalidation_.Write(*this) = gfx::Rect(bounds());
      }
    }

    // Clear out previous directly composited image state - if the layer
    // qualifies we'll set up the state below.
    picture_layer_inputs_.directly_composited_image_default_raster_scale =
        gfx::Vector2dF();
    picture_layer_inputs_.nearest_neighbor = false;
    std::optional<DisplayItemList::DirectlyCompositedImageResult> result =
        picture_layer_inputs_.display_list->GetDirectlyCompositedImageResult();
    if (result) {
      // Directly composited images are not guaranteed to fully cover every
      // pixel in the layer due to ceiling when calculating the tile content
      // rect from the layer bounds.
      recording_source->SetRequiresClear(true);
      picture_layer_inputs_.directly_composited_image_default_raster_scale =
          result->default_raster_scale;
      picture_layer_inputs_.nearest_neighbor = result->nearest_neighbor;
    }

    recording_source->UpdateDisplayItemList(
        picture_layer_inputs_.display_list,
        layer_tree_host()->recording_scale_factor());

    SetNeedsPushProperties();
    IncreasePaintCount();
  } else {
    // If this invalidation did not affect the recording source, then it can be
    // cleared as an optimization.
    last_updated_invalidation_.Write(*this).Clear();
  }

  return updated;
}

sk_sp<const SkPicture> PictureLayer::GetPicture() const {
  if (!draws_content() || bounds().IsEmpty())
    return nullptr;

  scoped_refptr<DisplayItemList> display_list =
      picture_layer_inputs_.client->PaintContentsToDisplayList();
  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(bounds().width(), bounds().height());
  canvas->clear(SK_ColorTRANSPARENT);
  display_list->Raster(canvas);
  return recorder.finishRecordingAsPicture();
}

void PictureLayer::ClearClient() {
  picture_layer_inputs_.client = nullptr;
  UpdateDrawsContent();
}

void PictureLayer::SetNearestNeighbor(bool nearest_neighbor) {
  if (picture_layer_inputs_.nearest_neighbor == nearest_neighbor)
    return;

  picture_layer_inputs_.nearest_neighbor = nearest_neighbor;
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
                                  std::vector<NodeInfo>* content) const {
  if (!draws_content())
    return;

  const DisplayItemList* display_item_list = GetDisplayItemList();
  if (!display_item_list)
    return;

  // We could run into this situation as CaptureContent could start at any time.
  if (transform_tree_index() == kInvalidPropertyNodeId)
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
  if (auto* outer_viewport_layer = layer_tree_host()->LayerByElementId(
          layer_tree_host()->OuterViewportScrollElementId())) {
    if (transform_tree_index() == outer_viewport_layer->transform_tree_index())
      return;
    gfx::Transform inverse_outer_screen_space_transform;
    if (!outer_viewport_layer->ScreenSpaceTransform().GetInverse(
            &inverse_outer_screen_space_transform)) {
      return;
    }
    gfx::Transform combined_transform =
        ScreenSpaceTransform() * inverse_outer_screen_space_transform;
    for (auto& i : *content) {
      i.visual_rect = MathUtil::ProjectEnclosingClippedRect(combined_transform,
                                                            i.visual_rect);
    }
  }
}

void PictureLayer::DropRecordingSourceContentIfInvalid(
    int source_frame_number) {
  gfx::Size recording_source_bounds = recording_source_.Read(*this)->GetSize();

  gfx::Size layer_bounds = bounds();

  // If update called, then recording source size must match bounds pushed to
  // impl layer.
  DCHECK(update_source_frame_number_.Read(*this) != source_frame_number ||
         layer_bounds == recording_source_bounds)
      << " bounds " << layer_bounds.ToString() << " recording source "
      << recording_source_bounds.ToString();

  if (update_source_frame_number_.Read(*this) != source_frame_number &&
      recording_source_bounds != layer_bounds) {
    // Update may not get called for the layer (if it's not in the viewport
    // for example), even though it has resized making the recording source no
    // longer valid. In this case just destroy the recording source.
    recording_source_.Write(*this)->SetEmptyBounds();
    picture_layer_inputs_.display_list = nullptr;
  }
}

const DisplayItemList* PictureLayer::GetDisplayItemList() const {
  return picture_layer_inputs_.display_list.get();
}

}  // namespace cc
