// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer_impl.h"

#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/append_quads_data.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"

namespace cc {

// static
std::unique_ptr<SurfaceLayerImpl> SurfaceLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    UpdateSubmissionStateCB update_submission_state_callback) {
  return base::WrapUnique(new SurfaceLayerImpl(
      tree_impl, id, std::move(update_submission_state_callback)));
}

// static
std::unique_ptr<SurfaceLayerImpl> SurfaceLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id) {
  return base::WrapUnique(new SurfaceLayerImpl(
      tree_impl, id, base::BindRepeating([](bool, base::WaitableEvent* event) {
        if (event)
          event->Signal();
      })));
}

SurfaceLayerImpl::SurfaceLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    UpdateSubmissionStateCB update_submission_state_callback)
    : LayerImpl(tree_impl, id),
      update_submission_state_callback_(
          std::move(update_submission_state_callback)) {}

SurfaceLayerImpl::~SurfaceLayerImpl() {
  // Do not call `update_submission_state_callback_` here.  There is only very
  // loose synchronization between when a layer gets a new impl layer and when
  // the old layer is destroyed.  For example, when a layer is moved to a new
  // tree, the old tree's impl layer might be destroyed after drawing has
  // started in the new tree with a new impl layer.  In that case, we'd be
  // clobbering the visibility state.  Instead, trust that SurfaceLayer has done
  // the right thing already.
}

mojom::LayerType SurfaceLayerImpl::GetLayerType() const {
  return mojom::LayerType::kSurface;
}

std::unique_ptr<LayerImpl> SurfaceLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return SurfaceLayerImpl::Create(tree_impl, id(),
                                  std::move(update_submission_state_callback_));
}

void SurfaceLayerImpl::SetRange(const viz::SurfaceRange& surface_range,
                                std::optional<uint32_t> deadline_in_frames) {
  if (surface_range_ == surface_range &&
      deadline_in_frames_ == deadline_in_frames) {
    return;
  }

  if (surface_range_.end() != surface_range.end() &&
      surface_range.end().local_surface_id().is_valid()) {
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Embed.Flow",
        TRACE_ID_GLOBAL(
            surface_range.end().local_surface_id().embed_trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
        "ImplSetSurfaceId", "surface_id", surface_range.end().ToString());
  }

  surface_range_ = surface_range;
  deadline_in_frames_ = deadline_in_frames;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::SetStretchContentToFillBounds(bool stretch_content) {
  if (stretch_content_to_fill_bounds_ == stretch_content)
    return;

  stretch_content_to_fill_bounds_ = stretch_content;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::SetSurfaceHitTestable(bool surface_hit_testable) {
  if (surface_hit_testable_ == surface_hit_testable)
    return;

  surface_hit_testable_ = surface_hit_testable;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::SetHasPointerEventsNone(bool has_pointer_events_none) {
  if (has_pointer_events_none_ == has_pointer_events_none)
    return;

  has_pointer_events_none_ = has_pointer_events_none;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::SetIsReflection(bool is_reflection) {
  if (is_reflection_ == is_reflection)
    return;

  is_reflection_ = is_reflection;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::ResetStateForUpdateSubmissionStateCallback() {
  will_draw_needs_reset_ = true;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  SurfaceLayerImpl* layer_impl = static_cast<SurfaceLayerImpl*>(layer);
  layer_impl->SetRange(surface_range_, std::move(deadline_in_frames_));
  // Unless the client explicitly specifies otherwise, don't block on
  // |surface_range_| more than once.
  deadline_in_frames_ = 0u;
  layer_impl->SetStretchContentToFillBounds(stretch_content_to_fill_bounds_);
  layer_impl->SetSurfaceHitTestable(surface_hit_testable_);
  layer_impl->SetHasPointerEventsNone(has_pointer_events_none_);
  layer_impl->SetIsReflection(is_reflection_);

  if (layer_impl->IsActive() && will_draw_needs_reset_) {
    layer_impl->will_draw_ = false;
    will_draw_needs_reset_ = false;
  }
}

bool SurfaceLayerImpl::WillDraw(
    DrawMode draw_mode,
    viz::ClientResourceProvider* resource_provider) {
  bool will_draw = LayerImpl::WillDraw(draw_mode, resource_provider);
  // If we have a change in WillDraw (meaning that visibility has changed), we
  // want to inform the VideoFrameSubmitter to start or stop submitting
  // compositor frames.
  if (will_draw_ != will_draw) {
    will_draw_ = will_draw;
    if (update_submission_state_callback_) {
      // If we're in synchronous composite mode, ensure that we finish running
      // the update submission state callback. This is important to avoid race
      // conditions in web_tests which results from a thread hop that happens in
      // the callback.
      if (layer_tree_impl()->IsInSynchronousComposite()) {
        base::WaitableEvent event;
        update_submission_state_callback_.Run(will_draw, &event);
        event.Wait();
      } else {
        update_submission_state_callback_.Run(will_draw, nullptr);
      }
    }
  }

  return will_draw;
}

void SurfaceLayerImpl::AppendQuads(viz::CompositorRenderPass* render_pass,
                                   AppendQuadsData* append_quads_data) {
  AppendRainbowDebugBorder(render_pass);

  float device_scale_factor = layer_tree_impl()->device_scale_factor();

  gfx::Rect quad_rect(gfx::ScaleToEnclosingRect(
      gfx::Rect(bounds()), device_scale_factor, device_scale_factor));
  gfx::Rect visible_quad_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          gfx::Rect(bounds()));

  visible_quad_rect = gfx::ScaleToEnclosingRect(
      visible_quad_rect, device_scale_factor, device_scale_factor);
  visible_quad_rect = gfx::IntersectRects(quad_rect, visible_quad_rect);

  if (visible_quad_rect.IsEmpty())
    return;

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();

  PopulateScaledSharedQuadState(shared_quad_state, device_scale_factor,
                                contents_opaque());

  if (surface_range_.IsValid()) {
    auto* quad = render_pass->CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
    quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                 surface_range_, background_color(),
                 stretch_content_to_fill_bounds_);
    quad->is_reflection = is_reflection_;
    // Add the primary surface ID as a dependency.
    append_quads_data->activation_dependencies.push_back(surface_range_.end());
    if (deadline_in_frames_) {
      if (!append_quads_data->deadline_in_frames)
        append_quads_data->deadline_in_frames = 0u;
      append_quads_data->deadline_in_frames = std::max(
          *append_quads_data->deadline_in_frames, *deadline_in_frames_);
    } else {
      append_quads_data->use_default_lower_bound_deadline = true;
    }
  } else {
    auto* quad =
        render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                 background_color(), false /* force_anti_aliasing_off */);
  }

  // Unless the client explicitly specifies otherwise, don't block on
  // |surface_range_| more than once.
  deadline_in_frames_ = 0u;
}

bool SurfaceLayerImpl::is_surface_layer() const {
  return true;
}

gfx::Rect SurfaceLayerImpl::GetEnclosingVisibleRectInTargetSpace() const {
  return GetScaledEnclosingVisibleRectInTargetSpace(
      layer_tree_impl()->device_scale_factor());
}

void SurfaceLayerImpl::GetDebugBorderProperties(SkColor4f* color,
                                                float* width) const {
  if (color)
    *color = DebugColors::SurfaceLayerBorderColor();
  if (width)
    *width = DebugColors::SurfaceLayerBorderWidth(
        layer_tree_impl() ? layer_tree_impl()->device_scale_factor() : 1);
}

void SurfaceLayerImpl::AppendRainbowDebugBorder(
    viz::CompositorRenderPass* render_pass) {
  if (!ShowDebugBorders(DebugBorderType::SURFACE))
    return;

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state, contents_opaque());

  float border_width = DebugColors::SurfaceLayerBorderWidth(
      layer_tree_impl() ? layer_tree_impl()->device_scale_factor() : 1);

  auto colors = std::to_array<SkColor4f>({
      SkColor4f(1.0f, 0.0f, 0.0f, 0.5f),     // Red.
      SkColor4f(1.0f, 0.65f, 0.0f, 0.5f),    // Orange.
      SkColor4f(1.0f, 1.0f, 0.0f, 0.5f),     // Yellow.
      SkColor4f(0.0f, 0.5f, 0.0f, 0.5f),     // Green.
      SkColor4f(0.0f, 0.0f, 1.0f, 0.50f),    // Blue.
      SkColor4f(0.93f, 0.51f, 0.93f, 0.5f),  // Violet.
  });

  const int kStripeWidth = 300;
  const int kStripeHeight = 300;

  for (int i = 0;; ++i) {
    // For horizontal lines.
    int x = kStripeWidth * i;
    int width = std::min(kStripeWidth, bounds().width() - x - 1);

    // For vertical lines.
    int y = kStripeHeight * i;
    int height = std::min(kStripeHeight, bounds().height() - y - 1);

    gfx::Rect top(x, 0, width, border_width);
    gfx::Rect bottom(x, bounds().height() - border_width, width, border_width);
    gfx::Rect left(0, y, border_width, height);
    gfx::Rect right(bounds().width() - border_width, y, border_width, height);

    if (top.IsEmpty() && left.IsEmpty())
      break;

    if (!top.IsEmpty()) {
      bool force_anti_aliasing_off = false;
      auto* top_quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      top_quad->SetNew(shared_quad_state, top, top, colors[i % colors.size()],
                       force_anti_aliasing_off);

      auto* bottom_quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      bottom_quad->SetNew(shared_quad_state, bottom, bottom,
                          colors[colors.size() - 1 - (i % colors.size())],
                          force_anti_aliasing_off);

      if (contents_opaque()) {
        // Draws a stripe filling the layer vertically with the same color and
        // width as the horizontal stipes along the layer's top border.
        auto* solid_quad =
            render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
        // The inner fill is more transparent then the border.
        static const float kFillOpacity = 0.1f;
        SkColor4f fill_color = colors[i % colors.size()];
        fill_color.fA *= kFillOpacity;
        gfx::Rect fill_rect(x, 0, width, bounds().height());
        solid_quad->SetNew(shared_quad_state, fill_rect, fill_rect, fill_color,
                           force_anti_aliasing_off);
      }
    }
    if (!left.IsEmpty()) {
      bool force_anti_aliasing_off = false;
      auto* left_quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      left_quad->SetNew(shared_quad_state, left, left,
                        colors[colors.size() - 1 - (i % colors.size())],
                        force_anti_aliasing_off);

      auto* right_quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      right_quad->SetNew(shared_quad_state, right, right,
                         colors[i % colors.size()], force_anti_aliasing_off);
    }
  }
}

void SurfaceLayerImpl::AsValueInto(base::trace_event::TracedValue* dict) const {
  LayerImpl::AsValueInto(dict);
  dict->SetString("surface_range", surface_range_.ToString());
}

}  // namespace cc
