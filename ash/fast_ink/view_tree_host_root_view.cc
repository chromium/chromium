// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/view_tree_host_root_view.h"

#include <GLES2/gl2.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/display_item_list.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/paint_context.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/views/widget/widget.h"

namespace ash {

struct ViewTreeHostRootView::Resource {
  Resource() = default;
  ~Resource() {
    gpu::SharedImageInterface* sii = context_provider->SharedImageInterface();
    DCHECK(!mailbox.IsZero());
    sii->DestroySharedImage(sync_token, mailbox);
  }
  scoped_refptr<viz::ContextProvider> context_provider;
  gpu::Mailbox mailbox;
  gpu::SyncToken sync_token;
  bool damaged = true;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
  int group_id = 1;
};

// An object holds a frame shink so that it can outlive the hosting
// widget and its view tree. This is necessary to release in-flight buffers
// (maintained in exported resources).
class ViewTreeHostRootView::LayerTreeViewTreeFrameSinkHolder
    : public cc::LayerTreeFrameSinkClient,
      public aura::WindowObserver {
 public:
  LayerTreeViewTreeFrameSinkHolder(
      ViewTreeHostRootView* view,
      std::unique_ptr<cc::LayerTreeFrameSink> frame_sink)
      : view_(view), frame_sink_(std::move(frame_sink)) {
    frame_sink_->BindToClient(this);
  }

  LayerTreeViewTreeFrameSinkHolder(const LayerTreeViewTreeFrameSinkHolder&) =
      delete;
  LayerTreeViewTreeFrameSinkHolder& operator=(
      const LayerTreeViewTreeFrameSinkHolder&) = delete;

  ~LayerTreeViewTreeFrameSinkHolder() override {
    if (frame_sink_)
      frame_sink_->DetachFromClient();
    if (root_window_)
      root_window_->RemoveObserver(this);
  }

  // Delete frame sink after having reclaimed all exported resources.
  // TODO(reveman): Find a better way to handle deletion of in-flight resources.
  // https://crbug.com/765763
  static void DeleteWhenLastResourceHasBeenReclaimed(
      std::unique_ptr<LayerTreeViewTreeFrameSinkHolder> holder) {
    if (holder->last_frame_size_in_pixels_.IsEmpty()) {
      // Delete sink holder immediately if no frame has been submitted.
      DCHECK(holder->exported_resources_.empty());
      return;
    }

    // Submit an empty frame to ensure that pending release callbacks will be
    // processed in a finite amount of time.
    viz::CompositorFrame frame;
    frame.metadata.begin_frame_ack.frame_id =
        viz::BeginFrameId(viz::BeginFrameArgs::kManualSourceId,
                          viz::BeginFrameArgs::kStartingFrameNumber);
    frame.metadata.begin_frame_ack.has_damage = true;
    frame.metadata.device_scale_factor =
        holder->last_frame_device_scale_factor_;
    frame.metadata.frame_token = ++holder->next_frame_token_;
    auto pass = viz::CompositorRenderPass::Create();
    pass->SetNew(viz::CompositorRenderPassId{1},
                 gfx::Rect(holder->last_frame_size_in_pixels_),
                 gfx::Rect(holder->last_frame_size_in_pixels_),
                 gfx::Transform());
    frame.render_pass_list.push_back(std::move(pass));
    holder->frame_sink_->SubmitCompositorFrame(std::move(frame),
                                               /*hit_test_data_changed=*/true);

    // Delete sink holder immediately if not waiting for exported resources to
    // be reclaimed.
    if (holder->exported_resources_.empty())
      return;

    // Delete sink holder immediately if native window is already gone.
    aura::Window* window = holder->view_->GetWidget()->GetNativeView();
    if (!window)
      return;

    aura::Window* root_window = window->GetRootWindow();
    holder->root_window_ = root_window;
    holder->view_ = nullptr;

    // If we have exported resources to reclaim then extend the lifetime of
    // holder by adding it as a root window observer. The holder will delete
    // itself when the root window is removed or when all exported resources
    // have been reclaimed.
    root_window->AddObserver(holder.release());
  }

  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             viz::ResourceId resource_id,
                             std::unique_ptr<Resource> resource) {
    exported_resources_[resource_id] = std::move(resource);
    last_frame_size_in_pixels_ = frame.size_in_pixels();
    last_frame_device_scale_factor_ = frame.metadata.device_scale_factor;
    frame.metadata.frame_token = ++next_frame_token_;
    frame_sink_->SubmitCompositorFrame(std::move(frame),
                                       /*hit_test_data_changed=*/true);
  }

  void DamageExportedResources() {
    for (auto& entry : exported_resources_)
      entry.second->damaged = true;
  }

  // Overridden from cc::LayerTreeFrameSinkClient:
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  absl::optional<viz::HitTestRegionList> BuildHitTestData() override {
    return {};
  }
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override {
    if (delete_pending_)
      return;
    for (auto& entry : resources) {
      auto it = exported_resources_.find(entry.id);
      DCHECK(it != exported_resources_.end());
      std::unique_ptr<Resource> resource = std::move(it->second);
      exported_resources_.erase(it);
      resource->sync_token = entry.sync_token;
      if (view_ && !entry.lost)
        view_->ReclaimResource(std::move(resource));
    }

    if (root_window_ && exported_resources_.empty())
      ScheduleDelete();
  }
  void SetTreeActivationCallback(base::RepeatingClosure callback) override {}
  void DidReceiveCompositorFrameAck() override {
    if (view_)
      view_->DidReceiveCompositorFrameAck();
  }
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override {
    if (view_)
      view_->DidPresentCompositorFrame(details.presentation_feedback);
  }
  void DidLoseLayerTreeFrameSink() override {
    exported_resources_.clear();
    if (root_window_)
      ScheduleDelete();
  }
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw,
              bool skip_draw) override {}
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override {}

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    root_window_->RemoveObserver(this);
    root_window_ = nullptr;
    // Make sure frame sink never outlives aura.
    frame_sink_->DetachFromClient();
    frame_sink_.reset();
    ScheduleDelete();
  }

 private:
  void ScheduleDelete() {
    if (delete_pending_)
      return;
    delete_pending_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                  this);
  }

  ViewTreeHostRootView* view_;
  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink_;
  base::flat_map<viz::ResourceId, std::unique_ptr<Resource>>
      exported_resources_;
  viz::FrameTokenGenerator next_frame_token_;
  gfx::Size last_frame_size_in_pixels_;
  float last_frame_device_scale_factor_ = 1.0f;
  aura::Window* root_window_ = nullptr;
  bool delete_pending_ = false;
};

ViewTreeHostRootView::ViewTreeHostRootView(views::Widget* widget)
    : views::internal::RootView(widget) {}

std::unique_ptr<ViewTreeHostRootView::Resource>
ViewTreeHostRootView::ObtainResource() {
  auto* window = GetWidget()->GetNativeView();
  if (!frame_sink_holder_) {
    frame_sink_holder_ = std::make_unique<LayerTreeViewTreeFrameSinkHolder>(
        this, window->CreateLayerTreeFrameSink());
  }

  float dsf = window->GetHost()->device_scale_factor();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);

  gfx::Size new_size = gfx::ScaleToCeiledSize(size(), dsf);
  if (display.panel_rotation() == display::Display::ROTATE_90 ||
      display.panel_rotation() == display::Display::ROTATE_270) {
    new_size.SetSize(new_size.height(), new_size.width());
  }

  rotate_transform_.MakeIdentity();

  switch (display.panel_rotation()) {
    case display::Display::ROTATE_0:
      break;
    case display::Display::ROTATE_90:
      rotate_transform_.Translate(new_size.width(), 0);
      rotate_transform_.Rotate(90);
      break;
    case display::Display::ROTATE_180:
      rotate_transform_.Translate(new_size.width(), new_size.height());
      rotate_transform_.Rotate(180);
      break;
    case display::Display::ROTATE_270:
      rotate_transform_.Translate(0, new_size.height());
      rotate_transform_.Rotate(270);
      break;
  }

  if (buffer_size_ != new_size) {
    buffer_size_ = new_size;
    // Clear All resources.
    resource_group_id_++;
    returned_resources_.clear();
  }

  if (!returned_resources_.empty()) {
    auto resource = std::move(returned_resources_.back());
    returned_resources_.pop_back();
    return resource;
  }
  auto resource = std::make_unique<Resource>();
  resource->group_id = resource_group_id_;

  gpu::GpuMemoryBufferManager* gmb_manager =
      aura::Env::GetInstance()->context_factory()->GetGpuMemoryBufferManager();
  resource->gpu_memory_buffer = gmb_manager->CreateGpuMemoryBuffer(
      buffer_size_,
      SK_B32_SHIFT ? gfx::BufferFormat::RGBA_8888
                   : gfx::BufferFormat::BGRA_8888,
      gfx::BufferUsage::SCANOUT_CPU_READ_WRITE, gpu::kNullSurfaceHandle,
      nullptr);
  if (!resource->gpu_memory_buffer) {
    LOG(ERROR) << "Failed to create GPU memory buffer";
    return nullptr;
  }

  resource->context_provider = aura::Env::GetInstance()
                                   ->context_factory()
                                   ->SharedMainThreadContextProvider();
  if (!resource->context_provider) {
    LOG(ERROR) << "Failed to acquire a context provider";
    return nullptr;
  }

  return resource;
}

ViewTreeHostRootView::~ViewTreeHostRootView() {
  if (frame_sink_holder_) {
    LayerTreeViewTreeFrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
        std::move(frame_sink_holder_));
  }
}

void ViewTreeHostRootView::Paint() {
  if (!GetWidget() || !GetWidget()->GetNativeWindow()) {
    SchedulePaintInRect(gfx::Rect());
    return;
  }

  // We make no attempts to recover if it failed to obtain a resource. It is
  // expected that this class is either short-lived, or for debugging and
  // requiring new instance to be created in lost context situations is
  // acceptable and keeps the code simple.
  auto resource = ObtainResource();
  if (!resource)
    return;

  DCHECK(pending_paint_);
  pending_paint_ = false;

  auto display_item_list = base::MakeRefCounted<cc::DisplayItemList>();
  float dsf = GetWidget()->GetCompositor()->device_scale_factor();
  ui::PaintContext context(display_item_list.get(), dsf, damaged_paint_rect_,
                           /*pixel_canvas=*/true);

  GetWidget()->OnNativeWidgetPaint(context);
  display_item_list->Finalize();

  if (!resource->gpu_memory_buffer->Map()) {
    TRACE_EVENT0("ui", "ViewTreeHostRootView::Paint::Map");
    LOG(ERROR) << "Failed to map GPU memory buffer";
    return;
  }
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(buffer_size_.width(), buffer_size_.height());
  uint8_t* data = static_cast<uint8_t*>(resource->gpu_memory_buffer->memory(0));
  int stride = resource->gpu_memory_buffer->stride(0);
  std::unique_ptr<SkCanvas> canvas =
      SkCanvas::MakeRasterDirect(info, data, stride);
  canvas->setMatrix(gfx::TransformToFlattenedSkMatrix(rotate_transform_));
  display_item_list->Raster(canvas.get());

  {
    TRACE_EVENT0("ui", "ViewTreeHostRootView::Paint::Unmap");
    // Unmap to flush writes to buffer.
    resource->gpu_memory_buffer->Unmap();
  }

  UpdateSurface(damaged_paint_rect_, std::move(resource));
  damaged_paint_rect_ = gfx::Rect();
}

void ViewTreeHostRootView::SchedulePaintInRect(const gfx::Rect& rect) {
  damaged_paint_rect_.Union(rect);

  if (pending_paint_)
    return;

  pending_paint_ = true;

  if (!pending_compositor_frame_ack_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ViewTreeHostRootView::Paint,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

bool ViewTreeHostRootView::GetIsOverlayCandidate() {
  return is_overlay_candidate_;
}

void ViewTreeHostRootView::SetIsOverlayCandidate(bool is_overlay_candidate) {
  is_overlay_candidate_ = is_overlay_candidate;
}

void ViewTreeHostRootView::UpdateSurface(const gfx::Rect& damage_rect,
                                         std::unique_ptr<Resource> resource) {
  damage_rect_.Union(damage_rect);
  pending_resource_ = std::move(resource);

  if (!damage_rect.IsEmpty()) {
    frame_sink_holder_->DamageExportedResources();
    for (auto& returned_resource : returned_resources_)
      returned_resource->damaged = true;
  }

  if (!pending_compositor_frame_ack_)
    SubmitCompositorFrame();
}

void ViewTreeHostRootView::SubmitCompositorFrame() {
  TRACE_EVENT1("ui", "ViewTreeHostRootView::SubmitCompositorFrame", "damage",
               damage_rect_.ToString());

  float device_scale_factor =
      GetWidget()->GetCompositor()->device_scale_factor();

  // TODO(crbug.com/1131623): Should this be ceil? Why do we choose floor?
  gfx::Size size_in_pixel =
      gfx::ToFlooredSize(gfx::ConvertSizeToPixels(size(), device_scale_factor));
  gfx::Rect output_rect(size_in_pixel);

  gfx::Rect quad_rect;
  quad_rect = gfx::Rect(buffer_size_);

  gfx::Rect damage_rect;

  // TODO(oshima): Support partial content update.
  damage_rect = gfx::ToEnclosingRect(
      gfx::ConvertRectToPixels(damage_rect_, device_scale_factor));
  damage_rect.Intersect(output_rect);
  damage_rect_ = gfx::Rect();

  std::unique_ptr<Resource> resource = std::move(pending_resource_);

  if (resource->damaged) {
    DCHECK(resource->context_provider);
    gpu::SharedImageInterface* sii =
        resource->context_provider->SharedImageInterface();
    if (resource->mailbox.IsZero()) {
      DCHECK(!resource->sync_token.HasData());
      const uint32_t usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                             gpu::SHARED_IMAGE_USAGE_SCANOUT;
      gpu::GpuMemoryBufferManager* gmb_manager =
          aura::Env::GetInstance()
              ->context_factory()
              ->GetGpuMemoryBufferManager();
      resource->mailbox = sii->CreateSharedImage(
          resource->gpu_memory_buffer.get(), gmb_manager, gfx::ColorSpace(),
          kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage);
    } else {
      sii->UpdateSharedImage(resource->sync_token, resource->mailbox);
    }
    resource->sync_token = sii->GenVerifiedSyncToken();
    resource->damaged = false;
  }

  viz::TransferableResource transferable_resource =
      viz::TransferableResource::MakeGpu(
          resource->mailbox, GL_LINEAR, GL_TEXTURE_2D, resource->sync_token,
          buffer_size_, SK_B32_SHIFT ? viz::RGBA_8888 : viz::BGRA_8888,
          is_overlay_candidate_);
  transferable_resource.id = id_generator_.GenerateNextId();

  gfx::Transform buffer_to_target_transform =
      rotate_transform_.GetCheckedInverse();

  const viz::CompositorRenderPassId kRenderPassId{1};
  auto render_pass = viz::CompositorRenderPass::Create();
  render_pass->SetNew(kRenderPassId, output_rect, damage_rect,
                      buffer_to_target_transform);

  viz::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(buffer_to_target_transform,
                     /*quad_layer_rect=*/output_rect,
                     /*visible_layer_rect=*/output_rect,
                     /*mask_filter_info=*/gfx::MaskFilterInfo(),
                     /*clip_rect=*/absl::nullopt, /*are_contents_opaque=*/false,
                     /*opacity=*/1.f,
                     /*blend_mode=*/SkBlendMode::kSrcOver,
                     /*sorting_context_id=*/0);

  viz::CompositorFrame frame;
  // TODO(eseckler): ViewTreeHostRootView should use BeginFrames and set
  // the ack accordingly.
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.device_scale_factor = device_scale_factor;

  frame.metadata.frame_token = ++next_frame_token_;

  viz::TextureDrawQuad* texture_quad =
      render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  gfx::RectF uv_crop(quad_rect);
  uv_crop.Scale(1.f / buffer_size_.width(), 1.f / buffer_size_.height());

  texture_quad->SetNew(
      quad_state, quad_rect, quad_rect,
      /*needs_blending=*/true, transferable_resource.id,
      /*premultiplied_alpha=*/true, uv_crop.origin(), uv_crop.bottom_right(),
      SkColors::kTransparent, vertex_opacity,
      /*y_flipped=*/false,
      /*nearest_neighbor=*/false,
      /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);
  texture_quad->set_resource_size_in_pixels(transferable_resource.size);
  frame.resource_list.push_back(transferable_resource);

  frame.render_pass_list.push_back(std::move(render_pass));
  frame_sink_holder_->SubmitCompositorFrame(
      std::move(frame), transferable_resource.id, std::move(resource));
}

void ViewTreeHostRootView::SubmitPendingCompositorFrame() {
  if (pending_resource_ && !pending_compositor_frame_ack_)
    SubmitCompositorFrame();
}

void ViewTreeHostRootView::DidReceiveCompositorFrameAck() {
  pending_compositor_frame_ack_ = false;

  if (pending_resource_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ViewTreeHostRootView::SubmitPendingCompositorFrame,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ViewTreeHostRootView::DidPresentCompositorFrame(
    const gfx::PresentationFeedback& feedback) {
  if (!presentation_callback_.is_null())
    presentation_callback_.Run(feedback);
}

void ViewTreeHostRootView::ReclaimResource(std::unique_ptr<Resource> resource) {
  if (resource_group_id_ == resource->group_id)
    returned_resources_.push_back(std::move(resource));
}

}  // namespace ash
