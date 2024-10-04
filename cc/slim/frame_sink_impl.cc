// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/frame_sink_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "cc/slim/constants.h"
#include "cc/slim/delayed_scheduler.h"
#include "cc/slim/frame_sink_impl_client.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"

namespace cc::slim {

FrameSinkImpl::UploadedUIResource::UploadedUIResource() = default;
FrameSinkImpl::UploadedUIResource::~UploadedUIResource() = default;
FrameSinkImpl::UploadedUIResource::UploadedUIResource(
    const UploadedUIResource&) = default;
FrameSinkImpl::UploadedUIResource& FrameSinkImpl::UploadedUIResource::operator=(
    const UploadedUIResource&) = default;

FrameSinkImpl::FrameSinkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_associated_remote,
    mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
        client_receiver,
    scoped_refptr<viz::RasterContextProvider> context_provider,
    base::PlatformThreadId io_thread_id,
    std::unique_ptr<Scheduler> scheduler)
    : task_runner_(std::move(task_runner)),
      scheduler_(std::move(scheduler)),
      pending_compositor_frame_sink_associated_remote_(
          std::move(compositor_frame_sink_associated_remote)),
      pending_client_receiver_(std::move(client_receiver)),
      context_provider_(std::move(context_provider)),
      io_thread_id_(io_thread_id) {
  scheduler_->Initialize(this);
}

FrameSinkImpl::~FrameSinkImpl() {
  // Iterate a copy of the viz_resource_ids since `uploaded_resources_` might
  // be modified when `UIResourceReleased()` is called.
  // Also note that the DestroySharedImage() call in UIResourceRelease()
  // requires the `ClientSharedImage` stored in the to-be-released resource to
  // have precisely one reference. Therefore, it is advisable to avoid any
  // operation that might alter the `ClientSharedImage`'s refcount, e.g.
  // creating a full copy of `uploaded_resources_`.
  auto resource_ids = base::MakeFlatSet<viz::ResourceId>(
      uploaded_resources_, {},
      [](auto& resource_pair) { return resource_pair.second.viz_resource_id; });
  for (const auto& uploaded_resource_id : resource_ids) {
    resource_provider_.RemoveImportedResource(uploaded_resource_id);
  }
  resource_provider_.ShutdownAndReleaseAllResources();
}

void FrameSinkImpl::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  if (local_surface_id_ == local_surface_id) {
    return;
  }
  local_surface_id_ = local_surface_id;
  hit_test_region_list_.reset();
}

bool FrameSinkImpl::BindToClient(FrameSinkImplClient* client) {
  DCHECK(client);
  if (context_provider_) {
    context_provider_->AddObserver(this);
    auto result = context_provider_->BindToCurrentSequence();
    if (result != gpu::ContextResult::kSuccess) {
      context_provider_->RemoveObserver(this);
      context_provider_ = nullptr;
      return false;
    }
  }

  client_ = client;

  frame_sink_remote_.Bind(
      std::move(pending_compositor_frame_sink_associated_remote_));
  frame_sink_remote_.set_disconnect_handler(
      base::BindOnce(&FrameSinkImpl::OnContextLost, base::Unretained(this)));
  client_receiver_.Bind(std::move(pending_client_receiver_), task_runner_);

  frame_sink_ = frame_sink_remote_.get();
  frame_sink_->InitializeCompositorFrameSinkType(
      viz::mojom::CompositorFrameSinkType::kLayerTree);

#if BUILDFLAG(IS_ANDROID)
  std::vector<int32_t> thread_ids;
  thread_ids.push_back(base::PlatformThread::CurrentId());
  if (io_thread_id_ != base::kInvalidThreadId) {
    thread_ids.push_back(io_thread_id_);
  }
  frame_sink_->SetThreadIds(thread_ids);
#endif
  return true;
}

void FrameSinkImpl::OnContextLost() {
  client_->DidLoseLayerTreeFrameSink();
}

void FrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame) {
  if (needs_begin_frame_ == needs_begin_frame) {
    return;
  }
  needs_begin_frame_ = needs_begin_frame;
  scheduler_->SetNeedsBeginFrame(needs_begin_frame);
  frame_sink_->SetNeedsBeginFrame(needs_begin_frame);
}

void FrameSinkImpl::MaybeCompositeNow() {
  scheduler_->MaybeCompositeNow();
}

void FrameSinkImpl::UploadUIResource(cc::UIResourceId resource_id,
                                     cc::UIResourceBitmap resource_bitmap) {
  gfx::Size size = resource_bitmap.GetSize();
  TRACE_EVENT1("cc", "slim::FrameSinkImpl::UploadUIResource", "size",
               size.ToString());
  const gpu::Capabilities& caps = context_provider_->ContextCapabilities();
  if (size.width() > caps.max_texture_size ||
      size.height() > caps.max_texture_size) {
    LOG(ERROR) << "Size exceeds max texture size";
    return;
  }
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  switch (resource_bitmap.GetFormat()) {
    case cc::UIResourceBitmap::RGBA8:
      format = viz::PlatformColor::BestSupportedTextureFormat(caps);
      break;
    case cc::UIResourceBitmap::ALPHA_8:
      format = viz::SinglePlaneFormat::kALPHA_8;
      break;
    case cc::UIResourceBitmap::ETC1:
      format = viz::SinglePlaneFormat::kETC1;
      break;
  }

  // CreateSharedImage() with initial pixels doesn't support specifying
  // non-standard stride so data must be exactly the minimum size required to
  // hold all pixels.
  DCHECK_EQ(format.EstimatedSizeInBytes(size), resource_bitmap.SizeInBytes());

  UploadedUIResource uploaded_resource;
  auto* sii = context_provider_->SharedImageInterface();
  constexpr gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  gpu::SharedImageUsageSet shared_image_usage =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  uploaded_resource.shared_image =
      sii->CreateSharedImage({format, resource_bitmap.GetSize(), color_space,
                              shared_image_usage, "SlimCompositorUIResource"},
                             resource_bitmap.GetPixels());
  CHECK(uploaded_resource.shared_image);
  gpu::SyncToken sync_token = sii->GenUnverifiedSyncToken();

  // NOTE: This resource will never be used as an overlay, as we we hardcode
  // `is_overlay_candidate` to false. Hence, the texture target should always be
  // GL_TEXTURE_2D (other texture targets are needed only for overlays).
  uploaded_resource.viz_resource_id = resource_provider_.ImportResource(
      viz::TransferableResource::MakeGpu(
          uploaded_resource.shared_image, /*texture_target=*/GL_TEXTURE_2D,
          sync_token, resource_bitmap.GetSize(), format,
          /*is_overlay_candidate=*/false,
          viz::TransferableResource::ResourceSource::kUI),
      base::BindOnce(&FrameSinkImpl::UIResourceReleased, base::Unretained(this),
                     resource_id));
  uploaded_resource.size = resource_bitmap.GetSize();
  uploaded_resource.is_opaque = resource_bitmap.GetOpaque();

  DCHECK(!uploaded_resources_.contains(resource_id));
  uploaded_resources_.emplace(resource_id, uploaded_resource);
}

void FrameSinkImpl::UIResourceReleased(cc::UIResourceId ui_resource_id,
                                       const gpu::SyncToken& sync_token,
                                       bool is_lost) {
  auto itr = uploaded_resources_.find(ui_resource_id);
  CHECK(itr != uploaded_resources_.end(), base::NotFatalUntil::M130);
  auto* sii = context_provider_->SharedImageInterface();
  sii->DestroySharedImage(sync_token, std::move(itr->second.shared_image));
  uploaded_resources_.erase(itr);
}

void FrameSinkImpl::MarkUIResourceForDeletion(cc::UIResourceId resource_id) {
  auto itr = uploaded_resources_.find(resource_id);
  if (itr == uploaded_resources_.end()) {
    return;
  }
  resource_provider_.RemoveImportedResource(itr->second.viz_resource_id);
}

viz::ResourceId FrameSinkImpl::GetVizResourceId(cc::UIResourceId resource_id) {
  auto itr = uploaded_resources_.find(resource_id);
  if (itr == uploaded_resources_.end()) {
    return viz::kInvalidResourceId;
  }
  return itr->second.viz_resource_id;
}

bool FrameSinkImpl::IsUIResourceOpaque(cc::UIResourceId resource_id) {
  auto it = uploaded_resources_.find(resource_id);
  if (it == uploaded_resources_.end()) {
    return true;
  }
  return it->second.is_opaque;
}

gfx::Size FrameSinkImpl::GetUIResourceSize(cc::UIResourceId resource_id) {
  auto it = uploaded_resources_.find(resource_id);
  if (it == uploaded_resources_.end()) {
    return gfx::Size();
  }

  return it->second.size;
}

int FrameSinkImpl::GetMaxTextureSize() const {
  if (context_provider_) {
    return context_provider_->ContextCapabilities().max_texture_size;
  }
  return kSoftwareMaxTextureSize;
}

void FrameSinkImpl::DidReceiveCompositorFrameAck(
    std::vector<viz::ReturnedResource> resources) {
  ReclaimResources(std::move(resources));
  DCHECK_GT(num_unacked_frames_, 0u);
  num_unacked_frames_--;
  if (!num_unacked_frames_) {
    scheduler_->SetIsSwapThrottled(false);
  }
  client_->DidReceiveCompositorFrameAck();
}

void FrameSinkImpl::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  resource_provider_.ReceiveReturnsFromParent(std::move(resources));
}

void FrameSinkImpl::OnBeginFrame(
    const viz::BeginFrameArgs& begin_frame_args,
    const viz::FrameTimingDetailsMap& timing_details,
    bool frame_ack,
    std::vector<viz::ReturnedResource> resources) {
  if (features::IsOnBeginFrameAcksEnabled()) {
    if (frame_ack) {
      DidReceiveCompositorFrameAck(std::move(resources));
    } else if (!resources.empty()) {
      ReclaimResources(std::move(resources));
    }
  }

  // Note order here is expected to be in order w.r.t viz::FrameTokenGT. This
  // mostly holds because `FrameTimingDetailsMap` is a flat_map which is sorted.
  // However this doesn't hold when frame token wraps.
  for (const auto& pair : timing_details) {
    client_->DidPresentCompositorFrame(pair.first, pair.second);
  }

  scheduler_->OnBeginFrameFromViz(begin_frame_args);
}

bool FrameSinkImpl::DoBeginFrame(const viz::BeginFrameArgs& begin_frame_args) {
  if (num_unacked_frames_) {
    return false;
  }

  if (!local_surface_id_.is_valid()) {
    return false;
  }

  TRACE_EVENT0("cc", "slim::FrameSinkImpl::DoBeginFrame");
  viz::CompositorFrame frame;
  base::flat_set<viz::ResourceId> viz_resource_ids;
  viz::HitTestRegionList hit_test_region_list;
  if (!client_->BeginFrame(begin_frame_args, frame, viz_resource_ids,
                           hit_test_region_list)) {
    return false;
  }

  if (local_surface_id_ == last_submitted_local_surface_id_) {
    DCHECK_EQ(last_submitted_device_scale_factor_, frame.device_scale_factor());
    DCHECK_EQ(last_submitted_size_in_pixels_.height(),
              frame.size_in_pixels().height());
    DCHECK_EQ(last_submitted_size_in_pixels_.width(),
              frame.size_in_pixels().width());
  }

  resource_provider_.PrepareSendToParent(std::move(viz_resource_ids).extract(),
                                         &frame.resource_list,
                                         context_provider_.get());

  bool send_new_hit_test_region_list = false;
  if (!hit_test_region_list_ ||
      !viz::HitTestRegionList::IsEqual(*hit_test_region_list_,
                                       hit_test_region_list)) {
    send_new_hit_test_region_list = true;
    hit_test_region_list_ = std::move(hit_test_region_list);
  }

  {
    TRACE_EVENT(
        "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
        perfetto::Flow::Global(begin_frame_args.trace_id),
        [&](perfetto::EventContext ctx) {
          auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
          auto* data = event->set_chrome_graphics_pipeline();
          data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                             StepName::STEP_SUBMIT_COMPOSITOR_FRAME);
          data->set_display_trace_id(begin_frame_args.trace_id);
        });
    frame_sink_->SubmitCompositorFrame(
        local_surface_id_, std::move(frame),
        send_new_hit_test_region_list ? hit_test_region_list_ : std::nullopt,
        0);
  }
  num_unacked_frames_++;
  if (num_unacked_frames_ == 1) {
    scheduler_->SetIsSwapThrottled(true);
  }
  client_->DidSubmitCompositorFrame();
  return true;
}

void FrameSinkImpl::SendDidNotProduceFrame(
    const viz::BeginFrameArgs& begin_frame_args) {
  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(begin_frame_args.trace_id),
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_DID_NOT_PRODUCE_FRAME);
        data->set_display_trace_id(begin_frame_args.trace_id);
      });
  frame_sink_->DidNotProduceFrame(viz::BeginFrameAck(begin_frame_args, false));
}

}  // namespace cc::slim
