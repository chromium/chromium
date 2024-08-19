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

scoped_refptr<PictureLayer> PictureLayer::Create(ContentLayerClient* client) {
  return base::WrapRefCounted(new PictureLayer(client));
}

PictureLayer::PictureLayer(ContentLayerClient* client)
    : client_(client),
      instrumentation_object_tracker_(id()),
      update_source_frame_number_(-1) {}

PictureLayer::~PictureLayer() = default;

std::unique_ptr<LayerImpl> PictureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return PictureLayerImpl::Create(tree_impl, id());
}

void PictureLayer::PushPropertiesTo(
    LayerImpl* base_layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  if (!update_rect().IsEmpty()) {
    layer_impl->set_has_non_animated_image_update_rect();
  }

  Layer::PushPropertiesTo(base_layer, commit_state, unsafe_state);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "PictureLayer::PushPropertiesTo");
  DropRecordingSourceContentIfInvalid(
      base_layer->layer_tree_impl()->source_frame_number());

  layer_impl->set_gpu_raster_max_texture_size(
      commit_state.device_viewport_rect.size());
  layer_impl->SetIsBackdropFilterMask(is_backdrop_filter_mask());

  layer_impl->UpdateRasterSource(CreateRasterSource(),
                                 &last_updated_invalidation_.Write(*this));
  DCHECK(last_updated_invalidation_.Read(*this).IsEmpty());
}

scoped_refptr<RasterSource> PictureLayer::CreateRasterSource() const {
  return recording_source_.Read(*this).CreateRasterSource();
}

void PictureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  Layer::SetLayerTreeHost(host);

  if (!host)
    return;

  recording_source_.Write(*this).SetSlowdownRasterScaleFactor(
      host->GetDebugState().slow_down_raster_scale_factor);

  // Source frame numbers are relative the LayerTreeHost, so this needs
  // to be reset.
  update_source_frame_number_.Write(*this) = -1;
}

void PictureLayer::SetNeedsDisplayRect(const gfx::Rect& layer_rect) {
  DCHECK(IsPropertyChangeAllowed());
  recording_source_.Write(*this).SetNeedsDisplayRect(layer_rect);
  Layer::SetNeedsDisplayRect(layer_rect);
}

bool PictureLayer::RequiresSetNeedsDisplayOnHdrHeadroomChange() const {
  if (const DisplayItemList* display_list = GetDisplayItemList()) {
    return display_list->content_color_usage() == gfx::ContentColorUsage::kHDR;
  }
  return false;
}

bool PictureLayer::Update() {
  update_source_frame_number_.Write(*this) =
      layer_tree_host()->SourceFrameNumber();
  bool updated = Layer::Update();

  auto& recording_source = recording_source_.Write(*this);
  recording_source.SetBackgroundColor(SafeOpaqueBackgroundColor());
  recording_source.SetRequiresClear(!contents_opaque() &&
                                    !client_->FillsBoundsCompletely());
  recording_source.SetCanUseRecordedBounds(
      layer_tree_host()->GetSettings().enable_hit_test_opaqueness);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"), "PictureLayer::Update",
               "source_frame_number", layer_tree_host()->SourceFrameNumber());
  devtools_instrumentation::ScopedLayerTreeTask update_layer(
      devtools_instrumentation::kUpdateLayer, id(), layer_tree_host()->GetId());

  // UpdateAndExpandInvalidation will give us an invalidation that covers
  // anything not explicitly recorded in this frame. We give this region
  // to the impl side so that it drops tiles that may not have a recording
  // for them.
  DCHECK(client_);

  updated |= recording_source.Update(
      bounds(), layer_tree_host()->recording_scale_factor(), *client_,
      last_updated_invalidation_.Write(*this));

  if (!updated) {
    return false;
  }

  SetNeedsPushProperties();
  IncreasePaintCount();
  return true;
}

sk_sp<const SkPicture> PictureLayer::GetPicture() const {
  if (!draws_content() || bounds().IsEmpty())
    return nullptr;

  scoped_refptr<DisplayItemList> display_list =
      client_->PaintContentsToDisplayList();
  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(bounds().width(), bounds().height());
  canvas->clear(SK_ColorTRANSPARENT);
  ScrollOffsetMap raster_inducing_scroll_offsets;
  const ScrollTree& scroll_tree =
      layer_tree_host()->property_trees()->scroll_tree();
  for (auto [element_id, _] : display_list->raster_inducing_scrolls()) {
    raster_inducing_scroll_offsets[element_id] =
        scroll_tree.current_scroll_offset(element_id);
  }
  display_list->Raster(canvas, /*image_provider=*/nullptr,
                       &raster_inducing_scroll_offsets);
  return recorder.finishRecordingAsPicture();
}

void PictureLayer::ClearClient() {
  client_ = nullptr;
  UpdateDrawsContent();
}

bool PictureLayer::HasDrawableContent() const {
  return client_ && Layer::HasDrawableContent();
}

void PictureLayer::SetIsBackdropFilterMask(bool is_backdrop_filter_mask) {
  if (is_backdrop_filter_mask_ == is_backdrop_filter_mask) {
    return;
  }

  is_backdrop_filter_mask_ = is_backdrop_filter_mask;
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
  gfx::Size recording_source_size = recording_source_.Read(*this).size();

  gfx::Size layer_bounds = bounds();

  // If update called, then recording source size must match bounds pushed to
  // impl layer.
  DCHECK(update_source_frame_number_.Read(*this) != source_frame_number ||
         layer_bounds == recording_source_size)
      << " bounds " << layer_bounds.ToString() << " recording source "
      << recording_source_size.ToString();

  if (update_source_frame_number_.Read(*this) != source_frame_number &&
      recording_source_size != layer_bounds) {
    // Update may not get called for the layer (if it's not in the viewport
    // for example), even though it has resized making the recording source no
    // longer valid. In this case just destroy the recording source.
    recording_source_.Write(*this).SetEmptyBounds();
  }
}

const DisplayItemList* PictureLayer::GetDisplayItemList() const {
  return recording_source_.Read(*this).display_list();
}

}  // namespace cc
