// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/layer_tree_impl.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/region.h"
#include "cc/slim/frame_data.h"
#include "cc/slim/frame_sink_impl.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree_client.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/frame_deadline.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc::slim {

LayerTreeImpl::PresentationCallbackInfo::PresentationCallbackInfo(
    uint32_t frame_token,
    std::vector<PresentationCallback> presentation_callbacks,
    std::vector<SuccessfulCallback> success_callbacks)
    : frame_token(frame_token),
      presentation_callbacks(std::move(presentation_callbacks)),
      success_callbacks(std::move(success_callbacks)) {}
LayerTreeImpl::PresentationCallbackInfo::~PresentationCallbackInfo() = default;
LayerTreeImpl::PresentationCallbackInfo::PresentationCallbackInfo(
    PresentationCallbackInfo&&) = default;
LayerTreeImpl::PresentationCallbackInfo&
LayerTreeImpl::PresentationCallbackInfo::operator=(PresentationCallbackInfo&&) =
    default;

LayerTreeImpl::LayerTreeImpl(LayerTreeClient* client,
                             uint32_t num_unneeded_begin_frame_before_stop,
                             int min_occlusion_tracking_dimension)
    : client_(client),
      num_unneeded_begin_frame_before_stop_(
          num_unneeded_begin_frame_before_stop),
      min_occlusion_tracking_dimension_(min_occlusion_tracking_dimension) {}

LayerTreeImpl::~LayerTreeImpl() {
  SetRoot(nullptr);
}

cc::UIResourceManager* LayerTreeImpl::GetUIResourceManager() {
  return &ui_resource_manager_;
}

void LayerTreeImpl::SetViewportRectAndScale(
    const gfx::Rect& device_viewport_rect,
    float device_scale_factor,
    const viz::LocalSurfaceId& local_surface_id) {
  if (local_surface_id_ != local_surface_id) {
    local_surface_id_ = local_surface_id;
    if (frame_sink_) {
      frame_sink_->SetLocalSurfaceId(local_surface_id);
    }
  }

  device_viewport_rect_ = device_viewport_rect;
  device_scale_factor_ = device_scale_factor;
  SetNeedsDraw();
}

void LayerTreeImpl::set_background_color(SkColor4f color) {
  if (background_color_ == color) {
    return;
  }

  background_color_ = color;
  SetNeedsDraw();
}

void LayerTreeImpl::SetVisible(bool visible) {
  if (visible_ == visible) {
    return;
  }

  visible_ = visible;
  MaybeRequestFrameSink();
  SetNeedsDraw();
}

bool LayerTreeImpl::IsVisible() const {
  return visible_;
}

void LayerTreeImpl::RequestPresentationTimeForNextFrame(
    PresentationCallback callback) {
  presentation_callback_for_next_frame_.emplace_back(std::move(callback));
}

void LayerTreeImpl::RequestSuccessfulPresentationTimeForNextFrame(
    SuccessfulCallback callback) {
  success_callback_for_next_frame_.emplace_back(std::move(callback));
}

void LayerTreeImpl::set_display_transform_hint(gfx::OverlayTransform hint) {
  display_transform_hint_ = hint;
}

void LayerTreeImpl::RequestCopyOfOutput(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  if (request->has_source()) {
    const base::UnguessableToken& source = request->source();
    auto it = base::ranges::find_if(
        copy_requests_for_next_frame_,
        [&source](const std::unique_ptr<viz::CopyOutputRequest>& x) {
          return x->has_source() && x->source() == source;
        });
    if (it != copy_requests_for_next_frame_.end()) {
      copy_requests_for_next_frame_.erase(it);
    }
  }
  copy_requests_for_next_frame_.push_back(std::move(request));
  SetNeedsDraw();
}

base::OnceClosure LayerTreeImpl::DeferBeginFrame() {
  num_defer_begin_frame_++;
  UpdateNeedsBeginFrame();
  return base::BindOnce(&LayerTreeImpl::ReleaseDeferBeginFrame,
                        weak_factory_.GetWeakPtr());
}

void LayerTreeImpl::ReleaseDeferBeginFrame() {
  DCHECK_GT(num_defer_begin_frame_, 0u);
  num_defer_begin_frame_--;
  UpdateNeedsBeginFrame();
}

void LayerTreeImpl::UpdateTopControlsVisibleHeight(float height) {
  if (top_controls_visible_height_ &&
      top_controls_visible_height_.value() == height) {
    return;
  }
  top_controls_visible_height_ = height;
  SetNeedsDraw();
}

void LayerTreeImpl::SetNeedsAnimate() {
  SetClientNeedsOneBeginFrame();
}

void LayerTreeImpl::SetNeedsRedraw() {
  SetClientNeedsOneBeginFrame();
}

const scoped_refptr<Layer>& LayerTreeImpl::root() const {
  return root_;
}

void LayerTreeImpl::SetRoot(scoped_refptr<Layer> root) {
  if (root_ == root) {
    return;
  }
  if (root_) {
    root_->SetLayerTree(nullptr);
  }
  root_ = std::move(root);
  if (root_) {
    root_->SetLayerTree(this);
    SetNeedsDraw();
  }
}

void LayerTreeImpl::SetFrameSink(std::unique_ptr<FrameSink> sink) {
  DCHECK(sink);
  frame_sink_.reset(static_cast<FrameSinkImpl*>(sink.release()));
  if (!frame_sink_->BindToClient(this)) {
    frame_sink_.reset();
    // This is equivalent to requesting another frame sink, so do not reset
    // `frame_sink_request_pending_` to avoid extra unexpected calls to
    // `RequestNewFrameSink`.
    client_->DidFailToInitializeLayerTreeFrameSink();
    return;
  }
  frame_sink_request_pending_ = false;
  if (local_surface_id_.is_valid()) {
    frame_sink_->SetLocalSurfaceId(local_surface_id_);
  }
  client_->DidInitializeLayerTreeFrameSink();
  ui_resource_manager_.RecreateUIResources();

  UpdateNeedsBeginFrame();
}

void LayerTreeImpl::ReleaseLayerTreeFrameSink() {
  DCHECK(!IsVisible());
  frame_sink_.reset();
}

bool LayerTreeImpl::BeginFrame(
    const viz::BeginFrameArgs& args,
    viz::CompositorFrame& out_frame,
    base::flat_set<viz::ResourceId>& out_resource_ids,
    viz::HitTestRegionList& out_hit_test_region_list) {
  // Skip any delayed BeginFrame messages that arrive even after we no longer
  // need it.
  if (!NeedsDraw()) {
    num_begin_frames_with_no_draw_++;
    frame_sink_->SetNeedsBeginFrame(NeedsBeginFrames());
    return false;
  }
  num_begin_frames_with_no_draw_ = 0u;

  // Unset `client_needs_one_begin_frame_` before BeginFrame. If client
  // requests more frames from inside the BeginFrame call, it's for the next
  // frame.
  client_needs_one_begin_frame_ = false;
  {
    base::AutoReset<bool> reset(&update_needs_begin_frame_pending_, true);
    client_->BeginFrame(args);
  }
  // Unset `needs_draw_` after client `BeginFrame`. Any layer or tree property
  // changes made by client `BeginFrame` are about to be drawn, so there is no
  // need for another frame.
  needs_draw_ = false;

  if (!root_ || device_viewport_rect_.IsEmpty()) {
    UpdateNeedsBeginFrame();
    return false;
  }

  GenerateCompositorFrame(args, out_frame, out_resource_ids,
                          out_hit_test_region_list);
  UpdateNeedsBeginFrame();
  return true;
}

void LayerTreeImpl::DidReceiveCompositorFrameAck() {
  client_->DidReceiveCompositorFrameAck();
}

void LayerTreeImpl::DidSubmitCompositorFrame() {
  client_->DidSubmitCompositorFrame();
}

void LayerTreeImpl::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  const bool success = !details.presentation_feedback.failed();
  for (auto itr = pending_presentation_callbacks_.begin();
       itr != pending_presentation_callbacks_.end();) {
    if (viz::FrameTokenGT(itr->frame_token, frame_token)) {
      break;
    }
    for (auto& callback : itr->presentation_callbacks) {
      std::move(callback).Run(details.presentation_feedback);
    }
    itr->presentation_callbacks.clear();

    // Only run `success_callbacks` if successful.
    if (success) {
      for (auto& callback : itr->success_callbacks) {
        std::move(callback).Run(details.presentation_feedback.timestamp);
      }
      itr->success_callbacks.clear();
    }
    // Keep the entry of `success_callbacks` is not empty, meaning this frame
    // wasn't successful, so that it can run on a subsequent successful frame.
    if (itr->success_callbacks.empty()) {
      itr = pending_presentation_callbacks_.erase(itr);
    } else {
      itr++;
    }
  }
}

void LayerTreeImpl::DidLoseLayerTreeFrameSink() {
  client_->DidLoseLayerTreeFrameSink();
  frame_sink_.reset();
  MaybeRequestFrameSink();
}

void LayerTreeImpl::NotifyTreeChanged() {
  SetNeedsDraw();
}

void LayerTreeImpl::NotifyPropertyChanged() {
  SetNeedsDraw();
}

viz::ClientResourceProvider* LayerTreeImpl::GetClientResourceProvider() {
  if (!frame_sink_) {
    return nullptr;
  }
  return frame_sink_->client_resource_provider();
}

viz::ResourceId LayerTreeImpl::GetVizResourceId(cc::UIResourceId id) {
  if (!frame_sink_) {
    return viz::kInvalidResourceId;
  }
  return frame_sink_->GetVizResourceId(id);
}

bool LayerTreeImpl::IsUIResourceOpaque(int resource_id) {
  return !frame_sink_ || frame_sink_->IsUIResourceOpaque(resource_id);
}

gfx::Size LayerTreeImpl::GetUIResourceSize(int resource_id) {
  if (!frame_sink_) {
    return gfx::Size();
  }

  return frame_sink_->GetUIResourceSize(resource_id);
}

void LayerTreeImpl::AddSurfaceRange(const viz::SurfaceRange& range) {
  DCHECK(range.IsValid());
  DCHECK(!referenced_surfaces_.contains(range));
  referenced_surfaces_.insert(range);
}

void LayerTreeImpl::RemoveSurfaceRange(const viz::SurfaceRange& range) {
  DCHECK(range.IsValid());
  DCHECK(referenced_surfaces_.contains(range));
  referenced_surfaces_.erase(range);
}

void LayerTreeImpl::MaybeRequestFrameSink() {
  if (frame_sink_ || !visible_ || frame_sink_request_pending_) {
    return;
  }
  frame_sink_request_pending_ = true;
  client_->RequestNewFrameSink();
}

void LayerTreeImpl::UpdateNeedsBeginFrame() {
  if (update_needs_begin_frame_pending_) {
    return;
  }

  if (frame_sink_ && NeedsBeginFrames()) {
    frame_sink_->SetNeedsBeginFrame(true);
  }
}

void LayerTreeImpl::SetClientNeedsOneBeginFrame() {
  client_needs_one_begin_frame_ = true;
  UpdateNeedsBeginFrame();
}

void LayerTreeImpl::SetNeedsDraw() {
  needs_draw_ = true;
  UpdateNeedsBeginFrame();
}

bool LayerTreeImpl::NeedsDraw() const {
  if (!visible_ || !frame_sink_ || num_defer_begin_frame_ > 0u) {
    return false;
  }
  return client_needs_one_begin_frame_ || needs_draw_;
}

bool LayerTreeImpl::NeedsBeginFrames() const {
  return NeedsDraw() ||
         num_begin_frames_with_no_draw_ < num_unneeded_begin_frame_before_stop_;
}

void LayerTreeImpl::GenerateCompositorFrame(
    const viz::BeginFrameArgs& args,
    viz::CompositorFrame& out_frame,
    base::flat_set<viz::ResourceId>& out_resource_ids,
    viz::HitTestRegionList& out_hit_test_region_list) {
  // TODO(crbug.com/1408128): Only has a very simple and basic compositor frame
  // generation. Some missing features include:
  // * Damage tracking
  TRACE_EVENT0("cc", "slim::LayerTreeImpl::ProduceFrame");

  for (auto& resource_request :
       ui_resource_manager_.TakeUIResourcesRequests()) {
    switch (resource_request.GetType()) {
      case cc::UIResourceRequest::UI_RESOURCE_CREATE:
        frame_sink_->UploadUIResource(resource_request.GetId(),
                                      resource_request.GetBitmap());
        break;
      case cc::UIResourceRequest::UI_RESOURCE_DELETE:
        frame_sink_->MarkUIResourceForDeletion(resource_request.GetId());
        break;
    }
  }

  out_hit_test_region_list.flags = viz::HitTestRegionFlags::kHitTestMine |
                                   viz::HitTestRegionFlags::kHitTestMouse |
                                   viz::HitTestRegionFlags::kHitTestTouch;
  out_hit_test_region_list.bounds = device_viewport_rect_;

  auto render_pass = viz::CompositorRenderPass::Create();
  render_pass->SetNew(viz::CompositorRenderPassId(root_->id()),
                      /*output_rect=*/device_viewport_rect_,
                      /*damage_rect=*/device_viewport_rect_,
                      /*transform_to_root_target=*/gfx::Transform());

  out_frame.metadata.frame_token = ++next_frame_token_;
  out_frame.metadata.begin_frame_ack =
      viz::BeginFrameAck(args, /*has_damage=*/true);
  out_frame.metadata.device_scale_factor = device_scale_factor_;
  out_frame.metadata.root_background_color = background_color_;
  out_frame.metadata.referenced_surfaces = std::vector<viz::SurfaceRange>(
      referenced_surfaces_.begin(), referenced_surfaces_.end());
  out_frame.metadata.top_controls_visible_height = top_controls_visible_height_;
  top_controls_visible_height_.reset();
  out_frame.metadata.display_transform_hint = display_transform_hint_;

  FrameData frame_data(out_frame, out_hit_test_region_list.regions);
  Draw(*root_, *render_pass, frame_data,
       /*parent_transform_to_root=*/gfx::Transform(),
       /*parent_transform_to_target=*/gfx::Transform(),
       /*parent_clip_in_target=*/nullptr, gfx::RectF(device_viewport_rect_),
       /*opacity=*/1.0f);

  if (background_color_.fA &&
      !frame_data.occlusion_in_target.Contains(device_viewport_rect_)) {
    // Quads does not cover entire viewport. Fill in the gutters.
    bool background_opaque = background_color_.isOpaque();
    render_pass->has_transparent_background = !background_opaque;
    Region unoccluded_region(device_viewport_rect_);
    for (size_t i = 0; i < frame_data.occlusion_in_target.GetRegionComplexity();
         ++i) {
      unoccluded_region.Subtract(frame_data.occlusion_in_target.GetRect(i));
    }
    if (!unoccluded_region.IsEmpty()) {
      viz::SharedQuadState* quad_state =
          render_pass->CreateAndAppendSharedQuadState();
      gfx::Rect gutter_bounding_rect = unoccluded_region.bounds();
      bool contents_opaque =
          background_opaque && unoccluded_region.GetRegionComplexity() <= 1;
      quad_state->SetAll(gfx::Transform(), gutter_bounding_rect,
                         gutter_bounding_rect, gfx::MaskFilterInfo(),
                         /*clip=*/absl::nullopt, contents_opaque,
                         /*opacity_f=*/1.0f, SkBlendMode::kSrcOver, 0);
      for (gfx::Rect unoccluded_rect : unoccluded_region) {
        viz::SolidColorDrawQuad* quad =
            render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
        quad->SetNew(quad_state, unoccluded_rect, unoccluded_rect,
                     background_color_, /*anti_aliasing_off=*/false);
      }
    }
  }

  render_pass->copy_requests = std::move(copy_requests_for_next_frame_);
  copy_requests_for_next_frame_.clear();
  out_frame.render_pass_list.push_back(std::move(render_pass));
  out_frame.metadata.activation_dependencies =
      std::vector<viz::SurfaceId>(frame_data.activation_dependencies.begin(),
                                  frame_data.activation_dependencies.end());
  out_frame.metadata.deadline = viz::FrameDeadline(
      args.frame_time, frame_data.deadline_in_frames.value_or(0u),
      args.interval, frame_data.use_default_lower_bound_deadline);

  for (const auto& pass : out_frame.render_pass_list) {
    for (const auto* quad : pass->quad_list) {
      for (viz::ResourceId resource_id : quad->resources) {
        out_resource_ids.insert(resource_id);
      }
    }
  }

  if (!presentation_callback_for_next_frame_.empty() ||
      !success_callback_for_next_frame_.empty()) {
    pending_presentation_callbacks_.emplace_back(
        out_frame.metadata.frame_token,
        std::move(presentation_callback_for_next_frame_),
        std::move(success_callback_for_next_frame_));
  }
}

void LayerTreeImpl::Draw(Layer& layer,
                         viz::CompositorRenderPass& parent_pass,
                         FrameData& data,
                         const gfx::Transform& parent_transform_to_root,
                         const gfx::Transform& parent_transform_to_target,
                         const gfx::RectF* parent_clip_in_target,
                         const gfx::RectF& clip_in_parent,
                         float parent_opacity) {
  DCHECK(!clip_in_parent.IsEmpty());
  if (layer.hide_layer_and_subtree() || layer.opacity() == 0.0f) {
    return;
  }

  absl::optional<gfx::Transform> transform_from_parent =
      layer.ComputeTransformFromParent();
  // If a 2d transform isn't invertible, then it must map the whole 2d space to
  // a single line or pointer, neither is visible.
  if (!transform_from_parent) {
    DLOG(WARNING) << "Skipping layer subtree from non-invertible transform.";
    return;
  }

  // Compute new clip in layer space.
  gfx::RectF clip_in_layer = transform_from_parent->MapRect(clip_in_parent);
  if (layer.masks_to_bounds()) {
    clip_in_layer.Intersect(
        gfx::RectF(layer.bounds().width(), layer.bounds().height()));
  }
  if (clip_in_layer.IsEmpty()) {
    return;
  }

  gfx::Transform transform_to_target = parent_transform_to_target;
  gfx::Transform transform_to_root = parent_transform_to_root;
  {
    // new_transform = parent_transform x layer_to_parent.
    const gfx::Transform transform_to_parent = layer.ComputeTransformToParent();
    transform_to_target.PreConcat(transform_to_parent);
    transform_to_root.PreConcat(transform_to_parent);
  }

  {
    const bool is_root = root_.get() == &layer;
    const bool filters_needs_pass = layer.HasFilters() && !is_root;
    const bool clip_needs_pass =
        !is_root && layer.masks_to_bounds() &&
        !transform_to_target.Preserves2dAxisAlignment();
    const bool opacity_needs_pass =
        layer.opacity() != 1.0f && layer.GetNumDrawingLayersInSubtree() > 1;
    if (!filters_needs_pass && !clip_needs_pass && !opacity_needs_pass) {
      // Does not need new render pass.
      // Compute new clip in target space.
      gfx::RectF new_clip_in_target(gfx::SizeF(layer.bounds()));
      const gfx::RectF* clip_in_target = parent_clip_in_target;
      if (layer.masks_to_bounds()) {
        new_clip_in_target = transform_to_target.MapRect(new_clip_in_target);
        if (parent_clip_in_target) {
          new_clip_in_target.Intersect(*parent_clip_in_target);
        }
        if (!new_clip_in_target.Contains(gfx::RectF(parent_pass.output_rect))) {
          clip_in_target = &new_clip_in_target;
        }
      }

      DrawChildrenAndAppendQuads(
          layer, parent_pass, data, transform_to_root, transform_to_target,
          clip_in_target, clip_in_layer, parent_opacity * layer.opacity());
      return;
    }
  }

  std::unique_ptr<viz::CompositorRenderPass> new_pass;
  gfx::Rect new_pass_content_bounds;
  // Scale can be applied when drawing layers into the new pass, or when
  // drawing the new pass into its target pass. Generally prefer the former to
  // avoid visual artifacts when scaling the output of the new pass. Therefore
  // the space of the new pass is the space of the layer with
  // `scale_to_new_pass` applied.
  // Another way to think about this: to_parent is split into scale_to_new_pass
  // and new_pass_to_parent such that:
  // to_parent = new_pass_to_parent x scale_to_new_pass
  gfx::Vector2dF scale_to_new_pass;
  gfx::Transform transform_new_pass_to_parent_target;
  {
    // Compute `scale_to_new_pass` first.
    scale_to_new_pass = gfx::ComputeTransform2dScaleComponents(
        transform_to_root, /*fallback_value=*/1.0f);
    // Only allow content scale to scale down (to save memory). Slim
    // compositor does support any vector content that is then rastered, so
    // there is no need to scale up a render pass to avoid visual artifacts.
    scale_to_new_pass.SetToMin({1.0f, 1.0f});
    DCHECK_NE(scale_to_new_pass.x(), 0.0f);
    DCHECK_NE(scale_to_new_pass.y(), 0.0f);

    // Compute "from new pass" transforms from "from layer" transforms by
    // applying inverse scale.
    float inverse_scale_x = 1.0f / scale_to_new_pass.x();
    float inverse_scale_y = 1.0f / scale_to_new_pass.y();
    transform_new_pass_to_parent_target = transform_to_target;
    transform_new_pass_to_parent_target.Scale(inverse_scale_x, inverse_scale_y);
    gfx::Transform new_pass_transform_to_root = transform_to_root;
    new_pass_transform_to_root.Scale(inverse_scale_x, inverse_scale_y);

    // Target is the new pass, so transform is just a scale.
    transform_to_target =
        gfx::Transform::MakeScale(scale_to_new_pass.x(), scale_to_new_pass.y());

    // First clip in layer space, then transform to parent target space.
    new_pass_content_bounds.set_size(layer.bounds());
    new_pass_content_bounds.Intersect(gfx::ToEnclosedRect(clip_in_layer));
    new_pass_content_bounds =
        transform_to_target.MapRect(new_pass_content_bounds);
    // Clip to max texture size.
    int max_texture_size = frame_sink_->GetMaxTextureSize();
    new_pass_content_bounds.set_width(
        std::min(new_pass_content_bounds.width(), max_texture_size));
    new_pass_content_bounds.set_height(
        std::min(new_pass_content_bounds.height(), max_texture_size));

    new_pass = viz::CompositorRenderPass::Create();
    // Note output_rect and damage_rect are updated below.
    viz::CompositorRenderPassId new_pass_id(layer.id());
    new_pass->SetNew(new_pass_id, /*output_rect=*/new_pass_content_bounds,
                     /*damage_rect=*/new_pass_content_bounds,
                     new_pass_transform_to_root);
  }

  // If a new pass is created, then there is no target clip when drawing into
  // the new pass since the bounds of the new pass already has any necessary
  // clip applied.
  const gfx::RectF* clip_in_target = nullptr;
  SimpleEnclosedRegion occlusion_in_new_pass;
  {
    SimpleEnclosedRegion parent_pass_occlusion = data.occlusion_in_target;
    data.occlusion_in_target.Clear();
    DrawChildrenAndAppendQuads(layer, *new_pass, data, transform_to_root,
                               transform_to_target, clip_in_target,
                               clip_in_layer,
                               /*opacity=*/1.0f);
    occlusion_in_new_pass = data.occlusion_in_target;

    // Apply new pass's occlusion to parent pass.
    if (transform_new_pass_to_parent_target.Preserves2dAxisAlignment()) {
      DCHECK(transform_new_pass_to_parent_target.Is2dTransform());
      for (size_t i = 0; i < occlusion_in_new_pass.GetRegionComplexity(); ++i) {
        // Use ToEnclosedRect to avoid including extra pixels as occluded due to
        // rounding error.
        gfx::Rect occlusion_in_parent_target =
            gfx::ToEnclosedRect(transform_new_pass_to_parent_target.MapRect(
                gfx::RectF(occlusion_in_new_pass.GetRect(i))));
        parent_pass_occlusion.Union(occlusion_in_parent_target);
      }
    }
    data.occlusion_in_target = parent_pass_occlusion;
  }

  if (new_pass->quad_list.empty()) {
    // Throw away new pass if it has no quads.
    return;
  }
  viz::SharedQuadState* shared_quad_state =
      parent_pass.CreateAndAppendSharedQuadState();
  // Any clip introduced by this layer is already applied by the bounds of the
  // new pass, so only need to apply any clips in parents target that came
  // from parent.
  absl::optional<gfx::Rect> clip_opt;
  if (parent_clip_in_target) {
    clip_opt = gfx::ToEnclosingRect(*parent_clip_in_target);
  }
  const bool new_pass_contents_opaque =
      occlusion_in_new_pass.Contains(new_pass_content_bounds);
  shared_quad_state->SetAll(
      transform_new_pass_to_parent_target, new_pass_content_bounds,
      new_pass_content_bounds, gfx::MaskFilterInfo(), clip_opt,
      new_pass_contents_opaque, parent_opacity * layer.opacity(),
      SkBlendMode::kSrcOver, 0);
  auto* quad =
      parent_pass.CreateAndAppendDrawQuad<viz::CompositorRenderPassDrawQuad>();

  // Union through quad list in new pass to compute content rect.
  gfx::Rect content_rect;
  for (const auto* new_pass_quad : new_pass->quad_list) {
    content_rect.Union(
        new_pass_quad->shared_quad_state->quad_to_target_transform.MapRect(
            new_pass_quad->rect));
  }
  content_rect.Intersect(new_pass_content_bounds);

  gfx::RectF tex_coord_rect(gfx::Rect(content_rect.size()));
  quad->SetAll(shared_quad_state, content_rect, content_rect,
               /*needs_blending=*/true, new_pass->id,
               /*mask_resource_id=*/viz::kInvalidResourceId,
               /*mask_uv_rect=*/gfx::RectF(),
               /*mask_texture_size=*/gfx::Size(),
               /*filters_scale=*/scale_to_new_pass,
               /*filters_origin=*/gfx::PointF(), tex_coord_rect,
               /*force_anti_aliasing_off=*/false,
               /*backdrop_filter_quality=*/1.f,
               /*intersects_damage_under=*/true);

  // TODO(crbug.com/1408128): Properly implement damage, including setting
  // `has_damage_from_contributing_content`.
  new_pass->output_rect = content_rect;
  new_pass->damage_rect = content_rect;
  data.frame.render_pass_list.push_back(std::move(new_pass));
}

void LayerTreeImpl::DrawChildrenAndAppendQuads(
    Layer& layer,
    viz::CompositorRenderPass& render_pass,
    FrameData& data,
    const gfx::Transform& transform_to_root,
    const gfx::Transform& transform_to_target,
    const gfx::RectF* clip_in_target,
    const gfx::RectF& clip_in_layer,
    float opacity) {
  for (auto& child : base::Reversed(layer.children())) {
    Draw(*child, render_pass, data, transform_to_root, transform_to_target,
         clip_in_target, clip_in_layer, opacity);
  }

  gfx::Rect integer_clip_in_target;
  if (clip_in_target) {
    integer_clip_in_target = gfx::ToEnclosingRect(*clip_in_target);
  }
  // Viz expects the visible rect to be a subrect of layer_rect (ie `bounds()`).
  // So intersect here unconditionally in case this layer is not
  // `masks_to_bounds()`.
  gfx::RectF clip(layer.bounds().width(), layer.bounds().height());
  clip.Intersect(clip_in_layer);
  if (!clip_in_layer.IsEmpty() && layer.HasDrawableContent() &&
      UpdateOcclusionRect(layer, data, transform_to_target, opacity, clip)) {
    layer.AppendQuads(render_pass, data, transform_to_root, transform_to_target,
                      clip_in_target ? &integer_clip_in_target : nullptr,
                      gfx::ToEnclosingRect(clip), opacity);
  }
}

bool LayerTreeImpl::UpdateOcclusionRect(
    Layer& layer,
    FrameData& data,
    const gfx::Transform& transform_to_target,
    float opacity,
    gfx::RectF& visible_rect) {
  if (opacity < 1.0f || !layer.contents_opaque()) {
    return true;
  }
  if (!transform_to_target.Preserves2dAxisAlignment()) {
    return true;
  }
  DCHECK(transform_to_target.Is2dTransform());
  DCHECK(transform_to_target.IsInvertible());

  gfx::RectF visible_rect_in_target = transform_to_target.MapRect(visible_rect);
  // Use enclosing rect here to avoid false rejections due to rounding error.
  if (data.occlusion_in_target.Contains(
          gfx::ToEnclosingRect(visible_rect_in_target))) {
    return false;
  }

  // Map occlusion to layer space and try to reduce `visible_rect`.
  gfx::Transform from_target;
  if (transform_to_target.GetInverse(&from_target)) {
    for (size_t i = 0; i < data.occlusion_in_target.GetRegionComplexity();
         ++i) {
      visible_rect.Subtract(
          from_target.MapRect(gfx::RectF(data.occlusion_in_target.GetRect(i))));
    }
  }

  // Add unoccluded visible rect to occlusion.
  if (visible_rect_in_target.width() >= min_occlusion_tracking_dimension_ ||
      visible_rect_in_target.height() >= min_occlusion_tracking_dimension_) {
    // Use ToEnclosedRect to avoid including extra pixels as occluded due to
    // rounding error.
    data.occlusion_in_target.Union(gfx::ToEnclosedRect(visible_rect_in_target));
  }
  return true;
}

}  // namespace cc::slim
