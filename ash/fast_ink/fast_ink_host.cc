// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_host.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
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
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace fast_ink {

// static
gfx::Rect FastInkHost::BufferRectFromWindowRect(
    const gfx::Transform& window_to_buffer_transform,
    const gfx::Size& buffer_size,
    const gfx::Rect& window_rect) {
  gfx::Rect buffer_rect = cc::MathUtil::MapEnclosingClippedRect(
      window_to_buffer_transform, window_rect);
  // Buffer rect is not bigger than actual buffer.
  buffer_rect.Intersect(gfx::Rect(buffer_size));
  return buffer_rect;
}

struct FastInkHost::Resource {
  Resource() = default;
  ~Resource() {
    // context_provider might be null in unit tests when ran with --mash
    // TODO(kaznacheev) Have MASH provide a context provider for tests
    // when https://crbug/772562 is fixed
    if (!context_provider)
      return;
    gpu::SharedImageInterface* sii = context_provider->SharedImageInterface();
    DCHECK(!mailbox.IsZero());
    sii->DestroySharedImage(sync_token, mailbox);
  }
  scoped_refptr<viz::ContextProvider> context_provider;
  gpu::Mailbox mailbox;
  gpu::SyncToken sync_token;
  bool damaged = true;
};

class FastInkHost::LayerTreeFrameSinkHolder
    : public cc::LayerTreeFrameSinkClient,
      public aura::WindowObserver {
 public:
  LayerTreeFrameSinkHolder(FastInkHost* host,
                           std::unique_ptr<cc::LayerTreeFrameSink> frame_sink)
      : host_(host), frame_sink_(std::move(frame_sink)) {
    frame_sink_->BindToClient(this);
  }
  ~LayerTreeFrameSinkHolder() override {
    if (frame_sink_)
      frame_sink_->DetachFromClient();
    if (root_window_)
      root_window_->RemoveObserver(this);
  }
  LayerTreeFrameSinkHolder(const LayerTreeFrameSinkHolder&) = delete;
  LayerTreeFrameSinkHolder& operator=(const LayerTreeFrameSinkHolder&) = delete;

  // Delete frame sink after having reclaimed all exported resources.
  // Returns false if the it should be released instead of reset and it will
  // self destruct.
  // TODO(reveman): Find a better way to handle deletion of in-flight resources.
  // https://crbug.com/765763
  bool DeleteWhenLastResourceHasBeenReclaimed() {
    if (last_frame_size_in_pixels_.IsEmpty()) {
      // Delete sink holder immediately if no frame has been submitted.
      DCHECK(exported_resources_.empty());
      return true;
    }

    // Submit an empty frame to ensure that pending release callbacks will be
    // processed in a finite amount of time.
    viz::CompositorFrame frame;
    frame.metadata.begin_frame_ack.frame_id =
        viz::BeginFrameId(viz::BeginFrameArgs::kManualSourceId,
                          viz::BeginFrameArgs::kStartingFrameNumber);
    frame.metadata.begin_frame_ack.has_damage = true;
    frame.metadata.device_scale_factor = last_frame_device_scale_factor_;
    frame.metadata.frame_token = ++next_frame_token_;
    auto pass = viz::CompositorRenderPass::Create();
    pass->SetNew(viz::CompositorRenderPassId{1},
                 gfx::Rect(last_frame_size_in_pixels_),
                 gfx::Rect(last_frame_size_in_pixels_), gfx::Transform());
    frame.render_pass_list.push_back(std::move(pass));
    frame_sink_->SubmitCompositorFrame(std::move(frame),
                                       /*hit_test_data_changed=*/true);

    // Delete sink holder immediately if not waiting for exported resources to
    // be reclaimed.
    if (exported_resources_.empty())
      return true;

    // If we have exported resources to reclaim then extend the lifetime of
    // holder by deleting it later.
    // itself when the root window is removed or when all exported resources
    // have been reclaimed.
    root_window_ = host_->host_window()->GetRootWindow();

    // This can be null during shutdown.
    if (!root_window_)
      return true;

    root_window_->AddObserver(this);
    host_ = nullptr;
    return false;
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
      if (host_ && !entry.lost)
        host_->ReclaimResource(std::move(resource));
    }

    if (root_window_ && exported_resources_.empty())
      ScheduleDelete();
  }
  void SetTreeActivationCallback(base::RepeatingClosure callback) override {}
  void DidReceiveCompositorFrameAck() override {
    if (host_)
      host_->DidReceiveCompositorFrameAck();
  }
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override {
    if (host_)
      host_->DidPresentCompositorFrame(details.presentation_feedback);
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

  void OnWindowDestroying(aura::Window* window) override {
    root_window_->RemoveObserver(this);
    root_window_ = nullptr;
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

  FastInkHost* host_;
  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink_;
  base::flat_map<viz::ResourceId, std::unique_ptr<Resource>>
      exported_resources_;
  viz::FrameTokenGenerator next_frame_token_;
  gfx::Size last_frame_size_in_pixels_;
  float last_frame_device_scale_factor_ = 1.0f;
  aura::Window* root_window_ = nullptr;
  bool delete_pending_ = false;
};

FastInkHost::FastInkHost(aura::Window* host_window,
                         const PresentationCallback& presentation_callback)
    : host_window_(host_window), presentation_callback_(presentation_callback) {
  // Take the root transform and apply this during buffer update instead of
  // leaving this up to the compositor. The benefit is that HW requirements
  // for being able to take advantage of overlays and direct scanout are
  // reduced significantly. Frames are submitted to the compositor with the
  // inverse transform to cancel out the transformation that would otherwise
  // be done by the compositor.
  window_to_buffer_transform_ = host_window_->GetHost()->GetRootTransform();
  gfx::Rect bounds(host_window_->GetBoundsInScreen().size());
  buffer_size_ =
      gfx::ToEnclosedRect(cc::MathUtil::MapClippedRect(
                              window_to_buffer_transform_,
                              gfx::RectF(bounds.width(), bounds.height())))
          .size();

  // Create a single GPU memory buffer. Content will be written into this
  // buffer without any buffering. The result is that we might be modifying
  // the buffer while it's being displayed. This provides minimal latency
  // but with potential tearing. Note that we have to draw into a temporary
  // surface and copy it into GPU memory buffer to avoid flicker.
  gpu_memory_buffer_ =
      aura::Env::GetInstance()
          ->context_factory()
          ->GetGpuMemoryBufferManager()
          ->CreateGpuMemoryBuffer(buffer_size_,
                                  SK_B32_SHIFT ? gfx::BufferFormat::RGBA_8888
                                               : gfx::BufferFormat::BGRA_8888,
                                  gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
                                  gpu::kNullSurfaceHandle, nullptr);
  LOG_IF(ERROR, !gpu_memory_buffer_) << "Failed to create GPU memory buffer";

  if (ash::switches::ShouldClearFastInkBuffer()) {
    bool map_result = gpu_memory_buffer_->Map();
    LOG_IF(ERROR, !map_result) << "Failed to map gpu buffer";
    uint8_t* memory = static_cast<uint8_t*>(gpu_memory_buffer_->memory(0));
    if (memory != nullptr) {
      gfx::Size size = gpu_memory_buffer_->GetSize();
      int stride = gpu_memory_buffer_->stride(0);
      // Clear the buffer before usage, since it may be uninitialized.
      // (http://b/168735625)
      for (int i = 0; i < size.height(); ++i)
        memset(memory + i * stride, 0, size.width() * 4);
    }
    gpu_memory_buffer_->Unmap();
  }

  frame_sink_holder_ = std::make_unique<LayerTreeFrameSinkHolder>(
      this, host_window_->CreateLayerTreeFrameSink());
}

FastInkHost::~FastInkHost() {
  if (!frame_sink_holder_->DeleteWhenLastResourceHasBeenReclaimed())
    frame_sink_holder_.release();
}

void FastInkHost::UpdateSurface(const gfx::Rect& content_rect,
                                const gfx::Rect& damage_rect,
                                bool auto_refresh) {
  content_rect_ = content_rect;
  damage_rect_.Union(damage_rect);
  auto_refresh_ = auto_refresh;
  pending_compositor_frame_ = true;

  if (!damage_rect.IsEmpty()) {
    frame_sink_holder_->DamageExportedResources();
    for (auto& resource : returned_resources_)
      resource->damaged = true;
  }

  if (!pending_compositor_frame_ack_)
    SubmitCompositorFrame();
}

void FastInkHost::SubmitCompositorFrame() {
  TRACE_EVENT1("ui", "FastInkHost::SubmitCompositorFrame", "damage",
               damage_rect_.ToString());

  float device_scale_factor = host_window_->layer()->device_scale_factor();

  gfx::Size window_size_in_dip = host_window_->GetBoundsInScreen().size();
  // TODO(crbug.com/1131619): Should this be ceil? Why do we choose floor?
  gfx::Size window_size_in_pixel = gfx::ToFlooredSize(
      gfx::ConvertSizeToPixels(window_size_in_dip, device_scale_factor));
  gfx::Rect output_rect(window_size_in_pixel);

  gfx::Rect quad_rect;
  gfx::Rect damage_rect;
  // Continuously redraw the full output rectangle when in auto-refresh mode.
  // This is necessary in order to allow single buffered updates without having
  // buffer changes outside the contents area cause artifacts.
  if (auto_refresh_) {
    quad_rect = gfx::Rect(buffer_size_);
    damage_rect = gfx::Rect(output_rect);
  } else {
    // Use minimal quad and damage rectangles when auto-refresh mode is off.
    quad_rect = BufferRectFromWindowRect(window_to_buffer_transform_,
                                         buffer_size_, content_rect_);
    damage_rect = gfx::ToEnclosingRect(
        gfx::ConvertRectToPixels(damage_rect_, device_scale_factor));
    damage_rect.Intersect(output_rect);
    pending_compositor_frame_ = false;
  }
  damage_rect_ = gfx::Rect();

  std::unique_ptr<Resource> resource;
  // Reuse returned resource if available.
  if (!returned_resources_.empty()) {
    resource = std::move(returned_resources_.back());
    returned_resources_.pop_back();
  }

  // Create new resource if needed.
  if (!resource)
    resource = std::make_unique<Resource>();

  if (resource->damaged) {
    // Acquire context provider for resource if needed.
    // Note: We make no attempts to recover if the context provider is later
    // lost. It is expected that this class is short-lived and requiring a
    // new instance to be created in lost context situations is acceptable and
    // keeps the code simple.
    if (!resource->context_provider) {
      resource->context_provider = aura::Env::GetInstance()
                                       ->context_factory()
                                       ->SharedMainThreadContextProvider();
      if (!resource->context_provider) {
        LOG(ERROR) << "Failed to acquire a context provider";
        return;
      }
    }

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
          gpu_memory_buffer_.get(), gmb_manager, gfx::ColorSpace(),
          kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage);
    } else {
      sii->UpdateSharedImage(resource->sync_token, resource->mailbox);
    }
    resource->sync_token = sii->GenVerifiedSyncToken();

    resource->damaged = false;
  }

  // Use HW overlay if continuous updates are expected.
  viz::TransferableResource transferable_resource =
      viz::TransferableResource::MakeGpu(
          resource->mailbox, GL_LINEAR, GL_TEXTURE_2D, resource->sync_token,
          buffer_size_, SK_B32_SHIFT ? viz::RGBA_8888 : viz::BGRA_8888,
          auto_refresh_);
  transferable_resource.id = id_generator_.GenerateNextId();

  gfx::Transform target_to_buffer_transform(window_to_buffer_transform_);
  target_to_buffer_transform.Scale(1.f / device_scale_factor,
                                   1.f / device_scale_factor);

  gfx::Transform buffer_to_target_transform =
      target_to_buffer_transform.GetCheckedInverse();

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
  // TODO(eseckler): FastInkHost should use BeginFrames and set the ack
  // accordingly.
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.device_scale_factor = device_scale_factor;

  viz::TextureDrawQuad* texture_quad =
      render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  gfx::RectF uv_crop(quad_rect);
  uv_crop.Scale(1.f / buffer_size_.width(), 1.f / buffer_size_.height());
  texture_quad->SetNew(
      quad_state, quad_rect, quad_rect,
      /*needs_blending=*/true, transferable_resource.id,
      /*premultiplied_alpha=*/true, uv_crop.origin(), uv_crop.bottom_right(),
      /*background_color=*/SkColors::kTransparent, vertex_opacity,
      /*y_flipped=*/false,
      /*nearest_neighbor=*/false,
      /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);
  texture_quad->set_resource_size_in_pixels(transferable_resource.size);
  frame.resource_list.push_back(transferable_resource);

  DCHECK(!pending_compositor_frame_ack_);
  pending_compositor_frame_ack_ = true;

  frame.render_pass_list.push_back(std::move(render_pass));
  frame_sink_holder_->SubmitCompositorFrame(
      std::move(frame), transferable_resource.id, std::move(resource));
}

void FastInkHost::SubmitPendingCompositorFrame() {
  if (pending_compositor_frame_ && !pending_compositor_frame_ack_)
    SubmitCompositorFrame();
}

void FastInkHost::DidReceiveCompositorFrameAck() {
  pending_compositor_frame_ack_ = false;
  if (pending_compositor_frame_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FastInkHost::SubmitPendingCompositorFrame,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void FastInkHost::DidPresentCompositorFrame(
    const gfx::PresentationFeedback& feedback) {
  if (!presentation_callback_.is_null())
    presentation_callback_.Run(feedback);
}

void FastInkHost::ReclaimResource(std::unique_ptr<Resource> resource) {
  returned_resources_.push_back(std::move(resource));
}

}  // namespace fast_ink
