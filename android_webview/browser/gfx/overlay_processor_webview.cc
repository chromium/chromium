// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/overlay_processor_webview.h"

#include <cstdlib>

#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/build_info.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/not_fatal_until.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "components/viz/service/display/resolved_frame_data.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "gpu/command_buffer/service/display_compositor_memory_and_task_controller_on_gpu.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/single_task_sequence.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace android_webview {
namespace {

constexpr gpu::CommandBufferNamespace kOverlayProcessorNamespace =
    gpu::CommandBufferNamespace::IN_PROCESS;

constexpr int kMaxBuffersInFlight = 3;

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    gpu::CommandBufferId command_buffer_id,
    gpu::SequenceId sequence_id) {
  return GpuServiceWebView::GetInstance()
      ->sync_point_manager()
      ->CreateSyncPointClientState(kOverlayProcessorNamespace,
                                   command_buffer_id, sequence_id);
}

}  // namespace

// Manages ASurfaceControl life-cycle and handles ASurfaceTransactions. Created
// on Android RenderThread, but both used on both Android RenderThread and GPU
// Main thread, so can be destroyed on one of them.
//
// Lifetime: WebView
// Each OverlayProcessorWebView owns one Manager. Ref-counted for callbacks.
class OverlayProcessorWebView::Manager
    : public base::RefCountedThreadSafe<OverlayProcessorWebView::Manager> {
 private:
  // Instances are either directly owned by Manager or indirectly through
  // OverlaySurface.
  class Resource {
   public:
    Resource(gpu::SharedImageManager* shared_image_manager,
             gpu::MemoryTypeTracker* memory_tracker,
             const gpu::Mailbox& mailbox,
             const gfx::RectF& uv_rect,
             const gfx::ColorSpace& color_space,
             float frame_rate,
             base::ScopedClosureRunner return_resource)
        : color_space_(color_space),
          frame_rate_(frame_rate),
          return_resource(std::move(return_resource)) {
      representation_ =
          shared_image_manager->ProduceOverlay(mailbox, memory_tracker);
      if (!representation_) {
        return;
      }

      read_access_ = representation_->BeginScopedReadAccess();
      if (!read_access_) {
        LOG(ERROR) << "Couldn't access shared image for read.";
        return;
      }

      gfx::GpuFenceHandle acquire_fence = read_access_->TakeAcquireFence();
      if (!acquire_fence.is_null()) {
        begin_read_fence_ = acquire_fence.Release();
      }

      AHardwareBuffer_Desc desc;
      base::AndroidHardwareBufferCompat::GetInstance().Describe(
          GetAHardwareBuffer(), &desc);
      gfx::RectF scaled_rect = gfx::ScaleRect(uv_rect, desc.width, desc.height);
      crop_rect_ = gfx::ToEnclosedRect(scaled_rect);
    }

    ~Resource() {
      DCHECK(!read_access_)
          << "Return() or ReturnUnused() must be called before dtor";
      DCHECK(!representation_);
    }

    Resource(Resource&&) = default;
    Resource& operator=(Resource&&) = default;

    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;

    void Return(base::ScopedFD end_read_fence) {
      // It's possible that we didn't have buffer for the first frame (see
      // `GetAHardwareBuffer()`) so there will be no read_access to set fence
      // to. On the other hand we shouldn't get a fence from flinger for this
      // surface in this case.
      if (read_access_) {
        gfx::GpuFenceHandle fence_handle;
        fence_handle.Adopt(std::move(end_read_fence));
        read_access_->SetReleaseFence(std::move(fence_handle));
        read_access_.reset();
      } else {
        DCHECK(!end_read_fence.is_valid());
      }
      representation_.reset();
    }

    void ReturnUnused() {
      read_access_.reset();
      representation_.reset();
      begin_read_fence_.reset();
    }

    base::ScopedFD TakeBeginReadFence() { return std::move(begin_read_fence_); }

    AHardwareBuffer* GetAHardwareBuffer() {
      // Note, that it's possible that BeginScopedReadAccess() will fail if
      // media couldn't get us a frame. We don't fail creation of resource in
      // this case, because if affects Surface acks and we don't to change the
      // frame submission flow. Instead we just set empty buffer to the surface.
      // Note, that it should only happen for the first frame in very rare
      // cases.
      if (!read_access_)
        return nullptr;

      DCHECK(representation_);
      DCHECK(read_access_);

      return read_access_->GetAHardwareBuffer();
    }

    const gfx::Rect& crop_rect() { return crop_rect_; }
    const gfx::ColorSpace& color_space() { return color_space_; }
    float frame_rate() const { return frame_rate_; }

   private:
    gfx::Rect crop_rect_;
    gfx::ColorSpace color_space_;
    float frame_rate_;
    base::ScopedClosureRunner return_resource;
    std::unique_ptr<gpu::OverlayImageRepresentation> representation_;
    std::unique_ptr<gpu::OverlayImageRepresentation::ScopedReadAccess>
        read_access_;
    base::ScopedFD begin_read_fence_;
  };

 public:
  Manager(gpu::CommandBufferId command_buffer_id, gpu::SequenceId sequence_id)
      : shared_image_manager_(
            GpuServiceWebView::GetInstance()->shared_image_manager()),
        memory_tracker_(std::make_unique<gpu::MemoryTypeTracker>(nullptr)),
        sync_point_client_state_(
            CreateSyncPointClientState(command_buffer_id, sequence_id)) {
    DETACH_FROM_THREAD(gpu_thread_checker_);
  }

  void SetGpuService(viz::GpuServiceImpl* gpu_service) {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);

    DCHECK_EQ(shared_image_manager_, gpu_service->shared_image_manager());
    gpu_task_runner_ = gpu_service->main_runner();
  }

  // Create SurfaceControl for |overlay_id| and set it up.
  void CreateOverlay(uint64_t overlay_id,
                     const viz::OverlayCandidate& candidate,
                     base::ScopedClosureRunner return_resource,
                     uint64_t sync_fence_release) {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
    TRACE_EVENT1("gpu,benchmark,android_webview",
                 "OverlayProcessorWebview::Manager::CreateOverlay",
                 "overlay_id", overlay_id);

    auto& transaction = GetHWUITransaction();
    // Use 0.f as unspecified frame rate will set proper frame rate on buffer
    // update.
    std::unique_ptr<Resource> resource = CreateResource(
        candidate.mailbox, candidate.unclipped_uv_rect, candidate.color_space,
        /*frame_rate=*/0.f, std::move(return_resource));

    {
      base::AutoLock lock(lock_);

      bool inserted;
      base::flat_map<uint64_t, OverlaySurface>::iterator it;
      std::tie(it, inserted) =
          overlay_surfaces_.emplace(overlay_id, GetParentSurface());
      DCHECK(inserted);
      auto& overlay_surface = it->second;

      UpdateGeometryInTransaction(transaction, *overlay_surface.bounds_surface,
                                  candidate);
      UpdateBufferInTransaction(transaction, *overlay_surface.buffer_surface,
                                resource.get());
      overlay_surface.buffer_update_pending = true;
    }

    DCHECK(!pending_resource_update_.contains(overlay_id));
    pending_resource_update_[overlay_id] = std::move(resource);

    sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
  }

  // Update geometry of SurfaceControl for |overlay_id|.
  void UpdateOverlayGeometry(uint64_t overlay_id,
                             const viz::OverlayCandidate& candidate) {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
    TRACE_EVENT1("gpu,benchmark,android_webview",
                 "OverlayProcessorWebview::Manager::UpdateOverlayGeometry",
                 "overlay_id", overlay_id);

    auto& transaction = GetHWUITransaction();

    base::AutoLock lock(lock_);
    auto& overlay_surface = GetOverlaySurfaceLocked(overlay_id);

    UpdateGeometryInTransaction(transaction, *overlay_surface.bounds_surface,
                                candidate);
  }

  // Update buffer in SurfaceControl for |overlay_id|. Called on GPU Main
  // Thread.
  void UpdateOverlayBuffer(uint64_t overlay_id,
                           gpu::Mailbox mailbox,
                           const gfx::ColorSpace& color_space,
                           const gfx::RectF& uv_rect,
                           float frame_rate,
                           base::ScopedClosureRunner return_resource) {
    DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
    TRACE_EVENT1("gpu,benchmark,android_webview",
                 "OverlayProcessorWebview::Manager::UpdateOverlayBuffer",
                 "overlay_id", overlay_id);

    base::AutoLock lock(lock_);
    auto& overlay_surface = GetOverlaySurfaceLocked(overlay_id);

    // If we're going to remove this overlay, there is no point in updating
    // buffer anymore. Resource will be unlocked by |return_resource| getting
    // out of scope.
    if (overlay_surface.pending_remove) {
      return;
    }

    std::unique_ptr<Resource> resource = CreateResource(
        mailbox, uv_rect, color_space, frame_rate, std::move(return_resource));

    // If there is already transaction with buffer update in-flight, store this
    // one. This will return any previous stored resource if any.
    if (overlay_surface.buffer_update_pending) {
      overlay_surface.SetPendingResource(std::move(resource));
      return;
    }

    SubmitTransactionWithBufferLocked(overlay_id, overlay_surface,
                                      std::move(resource));
  }

  // Initiate removal of SurfaceControl for |overlay_id|. Removal done in next
  // steps:
  // Unparent SurfaceControl, this happens synchronously with HWUI draw.
  // Set buffer to nullptr, to get current_buffer back with a fence.
  // Free SurfaceControl.
  void RemoveOverlay(uint64_t overlay_id) {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
    TRACE_EVENT1("gpu,benchmark,android_webview",
                 "OverlayProcessorWebview::Manager::RemoveOverlay",
                 "overlay_id", overlay_id);

    auto& transaction = GetHWUITransaction();
    {
      base::AutoLock lock(lock_);
      auto& overlay_surface = GetOverlaySurfaceLocked(overlay_id);
      transaction.SetParent(*overlay_surface.bounds_surface, nullptr);
    }

    pending_removals_.insert(overlay_id);
  }

  // Initiate removal of all current surfaces and drop reference to
  // parent_surface. This can be called with empty array.
  void RemoveOverlays(std::vector<uint64_t> overlay_ids) {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
    TRACE_EVENT0("gpu,benchmark,webview",
                 "OverlayProcessorWebview::Manager::RemoveOverlays");

    parent_surface_.reset();

    if (overlay_ids.empty())
      return;

    auto& transaction = GetHWUITransaction();
    {
      base::AutoLock lock(lock_);
      for (auto overlay_id : overlay_ids) {
        auto& overlay = GetOverlaySurfaceLocked(overlay_id);
        transaction.SetParent(*overlay.bounds_surface, nullptr);
      }
    }

    pending_removals_.insert(overlay_ids.begin(), overlay_ids.end());
  }

  void OnUpdateBufferTransactionAck(
      uint64_t overlay_id,
      std::unique_ptr<Resource> resource,
      gfx::SurfaceControl::TransactionStats transaction_stats) {
    DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
    TRACE_EVENT2(
        "gpu,benchmark,android_webview",
        "OverlayProcessorWebview::Manager::OnUpdateBufferTransactionAck",
        "overlay_id", overlay_id, "has_resource", !!resource);

    base::AutoLock lock(lock_);
    auto& overlay_surface = GetOverlaySurfaceLocked(overlay_id);

    DCHECK_EQ(transaction_stats.surface_stats.size(), 1u);
    DCHECK_EQ(transaction_stats.surface_stats.front().surface,
              overlay_surface.buffer_surface->surface());

    bool empty_buffer = !resource;
    overlay_surface.SetResource(
        std::move(resource),
        std::move(transaction_stats.surface_stats.front().fence));

    if (overlay_surface.pending_resource) {
      DCHECK(!overlay_surface.pending_remove);
      SubmitTransactionWithBufferLocked(
          overlay_id, overlay_surface,
          std::move(overlay_surface.pending_resource));
    }

    if (overlay_surface.pending_remove) {
      // If there is no resource, we can free our surface.
      if (empty_buffer) {
        overlay_surface.Reset();
        overlay_surfaces_.erase(overlay_id);
      } else {
        // This means there was buffer transaction in flight when surface was
        // hidden, we need to set buffer to nullptr, to free current one before
        // we can free the surface.
        SubmitTransactionWithBufferLocked(overlay_id, overlay_surface, nullptr);
      }
    }
  }

  void OnHWUITransactionAck(
      base::flat_map<uint64_t, std::unique_ptr<Resource>> resource_updates,
      base::flat_set<uint64_t> removes,
      gfx::SurfaceControl::TransactionStats transaction_stats) {
    DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
    TRACE_EVENT0("gpu,benchmark,android_webview",
                 "OverlayProcessorWebview::Manager::OnHWUITransactionAck");

    base::AutoLock lock(lock_);

    for (auto& update : resource_updates) {
      auto& overlay_surface = GetOverlaySurfaceLocked(update.first);

      base::ScopedFD fence;
      for (auto& stat : transaction_stats.surface_stats) {
        if (stat.surface == overlay_surface.buffer_surface->surface()) {
          DCHECK(!fence.is_valid());
          fence = std::move(stat.fence);
        }
      }

      overlay_surface.SetResource(std::move(update.second), std::move(fence));
      if (overlay_surface.pending_resource) {
        SubmitTransactionWithBufferLocked(
            update.first, overlay_surface,
            std::move(overlay_surface.pending_resource));
      }
    }

    for (auto& overlay_id : removes) {
      auto& overlay_surface = GetOverlaySurfaceLocked(overlay_id);
      overlay_surface.pending_remove = true;

      if (overlay_surface.pending_resource) {
        overlay_surface.pending_resource->ReturnUnused();
        overlay_surface.pending_resource.reset();
      }
      if (!overlay_surface.buffer_update_pending) {
        SubmitTransactionWithBufferLocked(overlay_id, overlay_surface, nullptr);
      }
    }
  }

  std::optional<gfx::SurfaceControl::Transaction> TakeHWUITransaction() {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);

    std::optional<gfx::SurfaceControl::Transaction> result;
    if (hwui_transaction_) {
      DCHECK(gpu_task_runner_);
      if (!pending_resource_update_.empty() || !pending_removals_.empty()) {
        auto cb = base::BindOnce(&Manager::OnHWUITransactionAck, this,
                                 std::move(pending_resource_update_),
                                 std::move(pending_removals_));
        hwui_transaction_->SetOnCompleteCb(std::move(cb), gpu_task_runner_);
      }

      result.swap(hwui_transaction_);
    }
    return result;
  }

 private:
  friend class OverlayProcessorWebView::ScopedSurfaceControlAvailable;
  friend class base::RefCountedThreadSafe<Manager>;

  // Class that holds SurfaceControl and associated resources.
  //
  // Instances are owned by Manager.
  class OverlaySurface {
   public:
    OverlaySurface(const gfx::SurfaceControl::Surface& parent)
        : bounds_surface(base::MakeRefCounted<gfx::SurfaceControl::Surface>(
              parent,
              "webview_overlay_bounds")),
          buffer_surface(base::MakeRefCounted<gfx::SurfaceControl::Surface>(
              *bounds_surface,
              "webview_overlay_content")) {}
    OverlaySurface(OverlaySurface&& other) = default;
    OverlaySurface& operator=(OverlaySurface&& other) = default;
    ~OverlaySurface() {
      DCHECK(!bounds_surface);
      DCHECK(!buffer_surface);
      DCHECK(!current_resource);
    }

    void SetResource(std::unique_ptr<Resource> resource,
                     base::ScopedFD end_read_fence) {
      if (current_resource) {
        current_resource->Return(std::move(end_read_fence));
      }
      current_resource = std::move(resource);

      DCHECK(buffer_update_pending);
      buffer_update_pending = false;
    }

    void SetPendingResource(std::unique_ptr<Resource> resource) {
      DCHECK(buffer_update_pending);
      if (pending_resource) {
        pending_resource->ReturnUnused();
      }
      pending_resource = std::move(resource);
    }

    void Reset() {
      DCHECK(!pending_resource);
      DCHECK(!current_resource);
      bounds_surface.reset();
      buffer_surface.reset();
    }

    // Set when we're in process of removing this overlay.
    bool pending_remove = false;

    // This is true when there is SurfaceControl transaction that affects buffer
    // of this overlay is in-flight.
    bool buffer_update_pending = false;

    // Resource that is currently latched by SurfaceControl
    std::unique_ptr<Resource> current_resource;

    // Resource that we want to send to SurfaceControl, but there was another
    // transaction with buffer update in-flight.
    std::unique_ptr<Resource> pending_resource;

    // SurfaceControl for this overlay.
    scoped_refptr<gfx::SurfaceControl::Surface> bounds_surface;
    scoped_refptr<gfx::SurfaceControl::Surface> buffer_surface;
  };

  ~Manager() {
    DCHECK(!hwui_transaction_);
    DCHECK(!parent_surface_);
  }

  gfx::SurfaceControl::Transaction& GetHWUITransaction() {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
    if (!hwui_transaction_)
      hwui_transaction_.emplace();
    return hwui_transaction_.value();
  }

  const gfx::SurfaceControl::Surface& GetParentSurface() {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
    if (!parent_surface_) {
      DCHECK(get_surface_control_);
      parent_surface_ =
          gfx::SurfaceControl::Surface::WrapUnowned(get_surface_control_());
      DCHECK(parent_surface_);
    }
    return *parent_surface_;
  }

  OverlaySurface& GetOverlaySurfaceLocked(uint64_t id) {
    lock_.AssertAcquired();
    auto surface = overlay_surfaces_.find(id);
    CHECK(surface != overlay_surfaces_.end(), base::NotFatalUntil::M130);
    return surface->second;
  }

  std::unique_ptr<Resource> CreateResource(
      const gpu::Mailbox& mailbox,
      const gfx::RectF uv_rect,
      const gfx::ColorSpace color_space,
      float frame_rate,
      base::ScopedClosureRunner return_resource) {
    if (mailbox.IsZero())
      return nullptr;
    return std::make_unique<Resource>(
        shared_image_manager_, memory_tracker_.get(), mailbox, uv_rect,
        color_space, frame_rate, std::move(return_resource));
  }

  // Because we update different parts of geometry on different threads we use
  // two surfaces to avoid races. The Bounds surface is setup the way it assumes
  // that its content size is kBoundsSurfaceContentSize. The Buffer surface is
  // setup to scale itself to kBoundsSurfaceContentSize. Note, that from scaling
  // perspective this number doesn't matter as scales of two surfaces will be
  // just multiplied inside SurfaceFlinger, but because positions and crop rects
  // are integers we need the content size to be large enough to avoid rounding
  // errors. To avoid floating point errors we also use power of two.
  static constexpr float kBoundsSurfaceContentSize = 8192.0f;

  static void UpdateGeometryInTransaction(
      gfx::SurfaceControl::Transaction& transaction,
      gfx::SurfaceControl::Surface& surface,
      const viz::OverlayCandidate& candidate) {
    DCHECK_EQ(absl::get<gfx::OverlayTransform>(candidate.transform),
              gfx::OVERLAY_TRANSFORM_NONE);
    gfx::Rect dst = gfx::ToEnclosingRect(candidate.unclipped_display_rect);

    transaction.SetPosition(surface, dst.origin());
    // Setup scale so the contents of size kBoundsSurfaceContentSize would fit
    // into display_rect. The buffer surface will make sure to scale its content
    // to kBoundsSurfaceContentSize.
    float scale_x = dst.width() / kBoundsSurfaceContentSize;
    float scale_y = dst.height() / kBoundsSurfaceContentSize;
    transaction.SetScale(surface, scale_x, scale_y);
    if (candidate.clip_rect) {
      // Make |crop_rect| relative to |display_rect|.
      auto crop_rect = dst;
      crop_rect.Intersect(*candidate.clip_rect);
      crop_rect.Offset(-dst.x(), -dst.y());

      // Crop rect is in content space, so we need to scale it.
      auto scaled_clip = gfx::ToEnclosingRect(gfx::ScaleRect(
          gfx::RectF(crop_rect), 1.0f / scale_x, 1.0f / scale_y));
      transaction.SetCrop(surface, scaled_clip);
    }
  }

  static void UpdateBufferInTransaction(
      gfx::SurfaceControl::Transaction& transaction,
      gfx::SurfaceControl::Surface& surface,
      Resource* resource) {
    TRACE_EVENT1("gpu,benchmark,android_webview",
                 "OverlayProcessorWebview::Manager::UpdateBufferInTransaction",
                 "has_resource", !!resource);
    auto* buffer = resource ? resource->GetAHardwareBuffer() : nullptr;
    if (buffer) {
      auto crop_rect = resource->crop_rect();

      // Crop rect defines the valid portion of the buffer, so we use its as a
      // surface size. This calculates scale from our size to bounds surface
      // content size, see comment at kBoundsSurfaceContentSize.
      float scale_x = kBoundsSurfaceContentSize / crop_rect.width();
      float scale_y = kBoundsSurfaceContentSize / crop_rect.height();

      // Crop rect is defined in buffer space, so we need to translate our
      // surface to make sure crop rect origin matches bounds surface (0, 0).
      // Position is defined in parent space, so we need to scale it.
      transaction.SetPosition(surface,
                              gfx::Point(-ceil(crop_rect.x() * scale_x),
                                         -ceil(crop_rect.y() * scale_y)));
      transaction.SetScale(surface, scale_x, scale_y);
      transaction.SetCrop(surface, crop_rect);
      transaction.SetColorSpace(surface, resource->color_space(), std::nullopt);
      transaction.SetBuffer(surface, buffer, resource->TakeBeginReadFence());

      if (gfx::SurfaceControl::SupportsSetFrameRate()) {
        transaction.SetFrameRate(surface, resource->frame_rate());
      }
    } else {
      // Android T has a bug where setting empty buffer to ASurfaceControl will
      // result in surface completely missing from ASurfaceTransactionStats in
      // OnComplete callback. To workaround it we create 1x1 buffer instead of
      // setting empty one.
      const bool need_empty_buffer_workaround =
          base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_T;
      if (need_empty_buffer_workaround) {
        // We never delete this buffer.
        static AHardwareBuffer* fake_buffer = nullptr;
        if (!fake_buffer) {
          AHardwareBuffer_Desc hwb_desc = {};
          hwb_desc.width = 1;
          hwb_desc.height = 1;
          hwb_desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
          hwb_desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
          hwb_desc.usage |= gfx::SurfaceControl::RequiredUsage();
          hwb_desc.layers = 1;

          // Allocate an AHardwareBuffer.
          base::AndroidHardwareBufferCompat::GetInstance().Allocate(
              &hwb_desc, &fake_buffer);
          if (!fake_buffer) {
            LOG(ERROR) << "Failed to allocate AHardwareBuffer";
          }
        }
        buffer = fake_buffer;
      }

      transaction.SetBuffer(surface, buffer, base::ScopedFD());
    }
  }

  void SubmitTransactionWithBufferLocked(uint64_t overlay_id,
                                         OverlaySurface& overlay_surface,
                                         std::unique_ptr<Resource> resource) {
    lock_.AssertAcquired();
    DCHECK(!overlay_surface.buffer_update_pending);
    DCHECK(gpu_task_runner_);
    overlay_surface.buffer_update_pending = true;

    gfx::SurfaceControl::Transaction transaction;
    UpdateBufferInTransaction(transaction, *overlay_surface.buffer_surface,
                              resource.get());

    auto cb = base::BindOnce(&Manager::OnUpdateBufferTransactionAck, this,
                             overlay_id, std::move(resource));
    transaction.SetOnCompleteCb(std::move(cb), gpu_task_runner_);
    transaction.Apply();
  }

  base::Lock lock_;

  // These can be accessed on any thread, but only initialized in ctor.
  const raw_ptr<gpu::SharedImageManager> shared_image_manager_;
  std::unique_ptr<gpu::MemoryTypeTracker> memory_tracker_;

  // GPU Main Thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // SyncPointClientState for render thread sequence.
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;

  // Can be accessed on both threads.
  base::flat_map<uint64_t, OverlaySurface> overlay_surfaces_ GUARDED_BY(lock_);

  // Pending updates for the current hwui transaction.
  base::flat_map<uint64_t, std::unique_ptr<Resource>> pending_resource_update_;
  base::flat_set<uint64_t> pending_removals_;

  scoped_refptr<gfx::SurfaceControl::Surface> parent_surface_;
  std::optional<gfx::SurfaceControl::Transaction> hwui_transaction_;

  GetSurfaceControlFn get_surface_control_ = nullptr;

  THREAD_CHECKER(render_thread_checker_);
  THREAD_CHECKER(gpu_thread_checker_);
};

OverlayProcessorWebView::OverlayProcessorWebView(
    viz::DisplayCompositorMemoryAndTaskController* display_controller,
    viz::FrameSinkManagerImpl* frame_sink_manager)
    : command_buffer_id_(gpu::DisplayCompositorMemoryAndTaskControllerOnGpu::
                             NextCommandBufferId()),
      render_thread_sequence_(display_controller->gpu_task_scheduler()),
      frame_sink_manager_(frame_sink_manager) {
  base::ScopedAllowBaseSyncPrimitives allow_wait;
  base::WaitableEvent event;
  render_thread_sequence_->ScheduleGpuTask(
      base::BindOnce(&OverlayProcessorWebView::CreateManagerOnRT,
                     base::Unretained(this), command_buffer_id_,
                     render_thread_sequence_->GetSequenceId(), &event),
      std::vector<gpu::SyncToken>());
  event.Wait();
}

OverlayProcessorWebView::~OverlayProcessorWebView() {
  render_thread_sequence_->ScheduleGpuTask(
      base::BindOnce(
          [](scoped_refptr<Manager> manager) {
            // manager leaves scope.
          },
          std::move(manager_)),
      std::vector<gpu::SyncToken>());
}

void OverlayProcessorWebView::CreateManagerOnRT(
    gpu::CommandBufferId command_buffer_id,
    gpu::SequenceId sequence_id,
    base::WaitableEvent* event) {
  manager_ = base::MakeRefCounted<Manager>(command_buffer_id, sequence_id);
  event->Signal();
}

void OverlayProcessorWebView::SetOverlaysEnabledByHWUI(bool enabled) {
  overlays_enabled_by_hwui_ = enabled;
}

void OverlayProcessorWebView::RemoveOverlays() {
  overlays_enabled_by_hwui_ = false;

  std::vector<uint64_t> ids;
  ids.reserve(overlays_.size());
  for (auto overlay : overlays_)
    ids.push_back(overlay.second.id);

  // Note, that we send it even there are no overlays, to drop reference to the
  // parent surface.
  render_thread_sequence_->ScheduleGpuTask(
      base::BindOnce(&Manager::RemoveOverlays, base::Unretained(manager_.get()),
                     std::move(ids)),
      std::vector<gpu::SyncToken>());

  overlays_.clear();
}

std::optional<gfx::SurfaceControl::Transaction>
OverlayProcessorWebView::TakeSurfaceTransactionOnRT() {
  DCHECK(manager_);
  return manager_->TakeHWUITransaction();
}

void OverlayProcessorWebView::CheckOverlaySupportImpl(
    const viz::OverlayProcessorInterface::OutputSurfaceOverlayPlane*
        primary_plane,
    viz::OverlayCandidateList* candidates) {
  // If HWUI doesn't want us to overlay, we shouldn't.
  if (!overlays_enabled_by_hwui_)
    return;

  // We need GpuServiceImpl (one for Gpu Main Thread, not GpuServiceWebView) to
  // use overlays. It takes time to initialize it, so we don't block
  // RenderThread for it. Instead we're just polling here if it's done.
  if (!gpu_thread_sequence_) {
    viz::GpuServiceImpl* gpu_service =
        VizCompositorThreadRunnerWebView::GetInstance()->GetGpuService();
    if (!gpu_service)
      return;

    gpu_thread_sequence_ = std::make_unique<gpu::SchedulerSequence>(
        gpu_service->GetGpuScheduler(), gpu_service->main_runner());

    render_thread_sequence_->ScheduleGpuTask(
        base::BindOnce(&OverlayProcessorWebView::Manager::SetGpuService,
                       base::Unretained(manager_.get()), gpu_service),
        std::vector<gpu::SyncToken>());
  }

  // Check candidates if they can be used with surface control.
  OverlayProcessorSurfaceControl::CheckOverlaySupportImpl(primary_plane,
                                                          candidates);
}

void OverlayProcessorWebView::TakeOverlayCandidates(
    viz::OverlayCandidateList* candidate_list) {
  overlay_candidates_.swap(*candidate_list);
  candidate_list->clear();
}

void OverlayProcessorWebView::ScheduleOverlays(
    viz::DisplayResourceProvider* resource_provider) {
  DCHECK(!resource_provider_ || resource_provider_ == resource_provider_);
  resource_provider_ = resource_provider;

  DCHECK(gpu_thread_sequence_ || overlay_candidates_.empty());

  base::flat_set<viz::FrameSinkId> seen;
  for (auto& candidate : overlay_candidates_) {
    viz::SurfaceId surface_id =
        resource_provider->GetSurfaceId(candidate.resource_id);

    viz::FrameSinkId sink_id = surface_id.frame_sink_id();
    seen.insert(sink_id);

    auto overlay = overlays_.find(sink_id);
    if (overlay != overlays_.end()) {
      // Need to update only geometry.
      render_thread_sequence_->ScheduleGpuTask(
          base::BindOnce(&Manager::UpdateOverlayGeometry,
                         base::Unretained(manager_.get()), overlay->second.id,
                         candidate),
          std::vector<gpu::SyncToken>());
      // If renderer embedded new surface (i.e video player size changed) we
      // need to update buffer here. For all other cases it's updated in
      // ProcessForFrameSinkId().
      if (overlay->second.surface_id != surface_id) {
        overlay->second.surface_id = surface_id;
        UpdateOverlayResource(sink_id, candidate.resource_id,
                              candidate.unclipped_uv_rect);
      }
    } else {
      overlay =
          overlays_
              .insert(std::make_pair(
                  sink_id, Overlay(next_overlay_id_++, candidate.resource_id,
                                   resource_provider_->GetChildId(
                                       candidate.resource_id))))
              .first;

      overlay->second.surface_id = surface_id;
      overlay->second.create_sync_token =
          gpu::SyncToken(kOverlayProcessorNamespace, command_buffer_id_,
                         ++sync_fence_release_);

      auto result = LockResource(overlay->second);
      candidate.mailbox = result.mailbox;

      render_thread_sequence_->ScheduleGpuTask(
          base::BindOnce(&Manager::CreateOverlay,
                         base::Unretained(manager_.get()), overlay->second.id,
                         candidate, std::move(result.unlock_cb),
                         overlay->second.create_sync_token.release_count()),
          {result.sync_token});
    }
  }

  for (auto it = overlays_.begin(); it != overlays_.end();) {
    if (!seen.contains(it->first)) {
      render_thread_sequence_->ScheduleGpuTask(
          base::BindOnce(&Manager::RemoveOverlay,
                         base::Unretained(manager_.get()), it->second.id),
          std::vector<gpu::SyncToken>());
      it = overlays_.erase(it);
    } else {
      ++it;
    }
  }
}

OverlayProcessorWebView::LockResult OverlayProcessorWebView::LockResource(
    Overlay& overlay) {
  LockResult result{};
  auto resource_id = overlay.resource_id;

  resource_lock_count_[overlay.surface_id.frame_sink_id()]++;

  OverlayResourceLock lock = OverlayResourceLock(
      static_cast<viz::DisplayResourceProviderSkia*>(resource_provider_),
      resource_id);
  result.sync_token = lock.sync_token();
  result.mailbox = lock.mailbox();

  locked_resources_.insert(std::make_pair(resource_id, std::move(lock)));

  auto return_cb = base::BindOnce(&OverlayProcessorWebView::ReturnResource,
                                  weak_ptr_factory_.GetWeakPtr(), resource_id,
                                  overlay.surface_id);
  auto return_cb_on_thread = base::BindPostTask(
      base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(return_cb));

  result.unlock_cb = base::ScopedClosureRunner(std::move(return_cb_on_thread));
  return result;
}

void OverlayProcessorWebView::UpdateOverlayResource(
    viz::FrameSinkId frame_sink_id,
    viz::ResourceId new_resource_id,
    const gfx::RectF& uv_rect) {
  DCHECK(resource_provider_);
  auto overlay = overlays_.find(frame_sink_id);
  CHECK(overlay != overlays_.end(), base::NotFatalUntil::M130);

  DCHECK(resource_provider_->IsOverlayCandidate(new_resource_id));

  if (new_resource_id != overlay->second.resource_id) {
    overlay->second.resource_id = new_resource_id;
    auto result = LockResource(overlay->second);

    gfx::ColorSpace color_space =
        OverlayProcessorSurfaceControl::GetOverrideColorSpace().value_or(
            resource_provider_->GetColorSpace(new_resource_id));

    gpu_thread_sequence_->ScheduleTask(
        base::BindOnce(&Manager::UpdateOverlayBuffer, manager_,
                       overlay->second.id, result.mailbox, color_space, uv_rect,
                       frame_rate_, std::move(result.unlock_cb)),
        {result.sync_token, overlay->second.create_sync_token});
  }
}

void OverlayProcessorWebView::ReturnResource(viz::ResourceId resource_id,
                                             viz::SurfaceId surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // |locked_resources_| is multimap and can contain multiple locks of the same
  // resource_id. There is no difference between locks at this point, they all
  // just keeping resource locked so it's safe to remove any of them when
  // OverlayManager return resources. When we delete last lock resource will be
  // return to the client.
  auto it = locked_resources_.find(resource_id);
  CHECK(it != locked_resources_.end(), base::NotFatalUntil::M130);
  locked_resources_.erase(it);

  DCHECK(resource_lock_count_.contains(surface_id.frame_sink_id()));
  auto& count = resource_lock_count_[surface_id.frame_sink_id()];
  DCHECK_GT(count, 0);

  // When the lock count reaches kMaxBuffersInFlight, we don't send acks to the
  // client in the ProcessForFrameSinkId. In this case we send ack here when the
  // lock count drops below the threshold. Note, that because we still lock
  // resource and schedule buffer update, the lock count can be larger than
  // kMaxBuffersInFlight in certain cases, like quick overlay demotion and
  // promotion again.
  if (count == kMaxBuffersInFlight) {
    auto* surface =
        frame_sink_manager_->surface_manager()->GetSurfaceForId(surface_id);
    if (surface) {
      surface->SendAckToClient();
    }
  }

  if (!--count)
    resource_lock_count_.erase(surface_id.frame_sink_id());
}

bool OverlayProcessorWebView::ProcessForFrameSinkId(
    const viz::FrameSinkId& frame_sink_id,
    const viz::ResolvedFrameData* frame_data) {
  auto it = overlays_.find(frame_sink_id);
  CHECK(it != overlays_.end(), base::NotFatalUntil::M130);
  auto& overlay = it->second;

  const auto& passes = frame_data->GetResolvedPasses();
  if (passes.empty()) {
    return false;
  }

  DCHECK_EQ(passes.size(), 1u);
  bool buffer_updated = false;

  auto& pass = passes.back();
  if (!pass.draw_quads().empty()) {
    DCHECK_EQ(pass.draw_quads().size(), 1u);
    auto* surface = frame_sink_manager_->surface_manager()->GetSurfaceForId(
        overlay.surface_id);

    // TODO(vasilyt): We should get this from surface aggregator after
    // aggregator refactoring will be finished.
    const auto& frame = surface->GetActiveFrame();
    auto* quad = frame.render_pass_list.back()->quad_list.front();

    if (gfx::SurfaceControl::SupportsSetFrameRate() &&
        base::FeatureList::IsEnabled(features::kWebViewFrameRateHints)) {
      float frame_rate = 0.f;
      const viz::FrameIntervalInputs& frame_interval_inputs =
          frame.metadata.frame_interval_inputs;
      std::optional<base::TimeDelta> frame_interval;
      for (const viz::ContentFrameIntervalInfo& content_info :
           frame_interval_inputs.content_interval_info) {
        if (!frame_interval) {
          frame_interval = content_info.frame_interval;
          continue;
        }
        if (frame_interval.value() != content_info.frame_interval) {
          frame_interval.reset();
          break;
        }
      }
      if (frame_interval &&
          frame_interval_inputs.has_only_content_frame_interval_updates) {
        frame_rate = frame_interval->ToHz();
      }
      constexpr float kEpsilon = 0.005;
      if (std::abs(frame_rate - frame_rate_) > kEpsilon) {
        frame_rate_ = frame_rate;
      }
    }

    // We overlay only TextureDrawQuads and only if resource
    // IsOverlayCandidate(), return false otherwise so we would trigger
    // invalidate and normal draw would remove this overlay candidate.
    if (quad->material == viz::TextureDrawQuad::kMaterial) {
      auto* texture_quad = viz::TextureDrawQuad::MaterialCast(quad);
      DCHECK(texture_quad->is_stream_video);

      auto uv_rect = gfx::BoundingRect(texture_quad->uv_top_left,
                                       texture_quad->uv_bottom_right);

      auto new_resource_id =
          pass.draw_quads().front().remapped_resources.ids[0];
      if (resource_provider_->IsOverlayCandidate(new_resource_id)) {
        UpdateOverlayResource(frame_sink_id, new_resource_id, uv_rect);
        buffer_updated = true;
      }
    }
    // If resource lock count reached kMaxBuffersInFlight it means we can't
    // schedule any more frames right away, in this case we delay sending ack to
    // the client and will send it in ReturnResources after OverlayManager will
    // process previous update.
    if (resource_lock_count_[frame_sink_id] < kMaxBuffersInFlight) {
      surface->SendAckToClient();
    }
  }

  return buffer_updated;
}

viz::SurfaceId OverlayProcessorWebView::GetOverlaySurfaceId(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = overlays_.find(frame_sink_id);
  if (it != overlays_.end()) {
    return it->second.surface_id;
  }
  return viz::SurfaceId();
}

bool OverlayProcessorWebView::IsFrameSinkOverlayed(
    viz::FrameSinkId frame_sink_id) {
  return overlays_.contains(frame_sink_id);
}

OverlayProcessorWebView::ScopedSurfaceControlAvailable::
    ScopedSurfaceControlAvailable(OverlayProcessorWebView* processor,
                                  GetSurfaceControlFn surface_getter)
    : processor_(processor) {
  DCHECK(processor_);
  DCHECK(processor_->manager_);
  processor_->manager_->get_surface_control_ = surface_getter;
}

OverlayProcessorWebView::ScopedSurfaceControlAvailable::
    ~ScopedSurfaceControlAvailable() {
  processor_->manager_->get_surface_control_ = nullptr;
}

OverlayProcessorWebView::Overlay::Overlay(uint64_t id,

                                          viz::ResourceId resource_id,
                                          int child_id)
    : id(id), resource_id(resource_id), child_id(child_id) {}

OverlayProcessorWebView::Overlay::Overlay(const Overlay&) = default;

OverlayProcessorWebView::Overlay::~Overlay() = default;

}  // namespace android_webview
