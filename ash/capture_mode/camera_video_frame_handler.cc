// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/camera_video_frame_handler.h"

#include <iostream>

#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/system/sys_info.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/system/buffer.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"

namespace ash {

namespace {

bool g_force_use_gpu_memory_buffer_for_test = false;

// A constant flag that describes which APIs the shared image mailboxes created
// for the video frame will be used with.
constexpr uint32_t kSharedImageUsage =
    gpu::SHARED_IMAGE_USAGE_GLES2 | gpu::SHARED_IMAGE_USAGE_RASTER |
    gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

// The usage of the GpuMemoryBuffer that backs the video frames on an actual
// device (of type `NATIVE_PIXMAP`). The buffer is going to be presented on the
// screen for rendering, will be used as a texture, and can be read by CPU and
// potentially a video encode accelerator.
constexpr gfx::BufferUsage kGpuMemoryBufferUsage =
    gfx::BufferUsage::SCANOUT_VEA_CPU_READ;

// The usage of the GpuMemoryBuffer that backs the video frames in unittests,
// since the type of that buffer will be `SHARED_MEMORY_BUFFER` which doesn't
// support the above on-device usage.
constexpr gfx::BufferUsage kGpuMemoryBufferUsageForTest =
    gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;

// The only supported video pixel format used on devices is `PIXEL_FORMAT_NV12`.
// This maps to a buffer format of `YUV_420_BIPLANAR`.
constexpr gfx::BufferFormat kGpuMemoryBufferFormat =
    gfx::BufferFormat::YUV_420_BIPLANAR;

// In unittests, the video pixel format used is `PIXEL_FORMAT_ARGB`, since the
// video frames are painted and verified manually using Skia. The buffer format
// used for this is `BGRA_8888`.
constexpr gfx::BufferFormat kGpuMemoryBufferFormatForTest =
    gfx::BufferFormat::BGRA_8888;

gfx::BufferUsage GetBufferUsage() {
  return g_force_use_gpu_memory_buffer_for_test ? kGpuMemoryBufferUsageForTest
                                                : kGpuMemoryBufferUsage;
}

gfx::BufferFormat GetBufferFormat() {
  return g_force_use_gpu_memory_buffer_for_test ? kGpuMemoryBufferFormatForTest
                                                : kGpuMemoryBufferFormat;
}

ui::ContextFactory* GetContextFactory() {
  return aura::Env::GetInstance()->context_factory();
}

bool IsGpuBufferTypeSupported(gfx::GpuMemoryBufferType type) {
  return type == gfx::NATIVE_PIXMAP || type == gfx::SHARED_MEMORY_BUFFER;
}

// Adjusts the requested video capture `params` depending on whether we're
// running on an actual device or the linux-chromeos build.
void AdjustParamsForCurrentConfig(media::VideoCaptureParams* params) {
  DCHECK(params);

  // The default params are good enough when running on linux-chromeos.
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !g_force_use_gpu_memory_buffer_for_test) {
    DCHECK_EQ(params->buffer_type,
              media::VideoCaptureBufferType::kSharedMemory);
    return;
  }

  // On an actual device, the camera HAL only supports NV12 pixel formats in a
  // GPU memory buffer.
  params->requested_format.pixel_format = media::PIXEL_FORMAT_NV12;
  params->buffer_type = media::VideoCaptureBufferType::kGpuMemoryBuffer;
}

// Creates and returns a list of the buffer planes for each we'll need to create
// a shared image and store it in `GpuMemoryBufferHandleHolder::mailboxes_`.
std::vector<gfx::BufferPlane> CreateGpuBufferPlanes() {
  std::vector<gfx::BufferPlane> planes;
  if (base::FeatureList::IsEnabled(
          media::kMultiPlaneVideoCaptureSharedImages)) {
    planes.push_back(gfx::BufferPlane::Y);
    planes.push_back(gfx::BufferPlane::UV);
  } else {
    planes.push_back(gfx::BufferPlane::DEFAULT);
  }
  return planes;
}

// Returns the buffer texture target used to create a `MailboxHolder` according
// to our GPU buffer usage, buffer format, and the given `context_capabilities`.
uint32_t CalculateBufferTextureTarget(
    const gpu::Capabilities& context_capabilities) {
  return gpu::GetBufferTextureTarget(GetBufferUsage(), GetBufferFormat(),
                                     context_capabilities);
}

bool IsFatalError(media::VideoCaptureError error) {
  switch (error) {
    case media::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread:
    case media::VideoCaptureError::kCrosHalV3DeviceDelegateMojoConnectionError:
    case media::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToOpenCameraDevice:
    case media::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice:
    case media::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToConfigureStreams:
    case media::VideoCaptureError::kCrosHalV3BufferManagerFatalDeviceError:
      return true;
    default:
      return false;
  }
}

// -----------------------------------------------------------------------------
// SharedMemoryBufferHandleHolder:

// Defines an implementation for a `BufferHandleHolder` that can extract a video
// frame that is backed by a `kSharedMemory` buffer type. This implementation is
// used only when running on a linux-chromeos build (a.k.a. the emulator).
class SharedMemoryBufferHandleHolder : public BufferHandleHolder {
 public:
  explicit SharedMemoryBufferHandleHolder(
      media::mojom::VideoBufferHandlePtr buffer_handle)
      : region_(std::move(buffer_handle->get_unsafe_shmem_region())) {
    DCHECK(buffer_handle->is_unsafe_shmem_region());
    DCHECK(!base::SysInfo::IsRunningOnChromeOS());
  }
  SharedMemoryBufferHandleHolder(const SharedMemoryBufferHandleHolder&) =
      delete;
  SharedMemoryBufferHandleHolder& operator=(
      const SharedMemoryBufferHandleHolder&) = delete;
  ~SharedMemoryBufferHandleHolder() override = default;

  // BufferHandleHolder:
  scoped_refptr<media::VideoFrame> OnFrameReadyInBuffer(
      video_capture::mojom::ReadyFrameInBufferPtr buffer) override {
    const size_t mapping_size = media::VideoFrame::AllocationSize(
        buffer->frame_info->pixel_format, buffer->frame_info->coded_size);
    if (!MaybeUpdateMapping(mapping_size))
      return {};

    auto& frame_info = buffer->frame_info;
    auto frame = media::VideoFrame::WrapExternalData(
        frame_info->pixel_format, frame_info->coded_size,
        frame_info->visible_rect, frame_info->visible_rect.size(),
        mapping_.GetMemoryAs<uint8_t>(), mapping_.size(),
        frame_info->timestamp);

    return frame;
  }

 private:
  // Maps a new region with a size `new_mapping_size` bytes if no `mapping_` is
  // available. Returns true if already mapped, or mapping is successful, false
  // otherwise.
  bool MaybeUpdateMapping(size_t new_mapping_size) {
    if (mapping_.IsValid()) {
      // TODO(https://crbug.com/1316812): What guarantees that this DCHECK will
      // hold?
      DCHECK_EQ(mapping_.size(), new_mapping_size);
      return true;
    }

    mapping_ = region_.Map();
    return mapping_.IsValid();
  }

  // The held shared memory region associated with this object.
  base::UnsafeSharedMemoryRegion region_;

  // Shared memory mapping associated with the held `region_`.
  base::WritableSharedMemoryMapping mapping_;
};

// -----------------------------------------------------------------------------
// GpuMemoryBufferHandleHolder:

// Defines an implementation for a `BufferHandleHolder` that can extract a video
// frame that is backed by a `kGpuMemoryBuffer` buffer type.
class GpuMemoryBufferHandleHolder : public BufferHandleHolder,
                                    public viz::ContextLostObserver {
 public:
  explicit GpuMemoryBufferHandleHolder(
      media::mojom::VideoBufferHandlePtr buffer_handle)
      : gpu_memory_buffer_handle_(
            std::move(buffer_handle->get_gpu_memory_buffer_handle())),
        buffer_planes_(CreateGpuBufferPlanes()),
        client_native_pixmap_factory_(
            ui::CreateClientNativePixmapFactoryOzone()),
        context_provider_(
            GetContextFactory()->SharedMainThreadContextProvider()),
        buffer_texture_target_(CalculateBufferTextureTarget(
            context_provider_->ContextCapabilities())) {
    DCHECK(buffer_handle->is_gpu_memory_buffer_handle());
    DCHECK(IsGpuBufferTypeSupported(gpu_memory_buffer_handle_.type));
    DCHECK(context_provider_);
    context_provider_->AddObserver(this);
  }

  GpuMemoryBufferHandleHolder(const GpuMemoryBufferHandleHolder&) = delete;
  GpuMemoryBufferHandleHolder& operator=(const GpuMemoryBufferHandleHolder&) =
      delete;

  ~GpuMemoryBufferHandleHolder() override {
    if (!context_provider_)
      return;

    context_provider_->RemoveObserver(this);

    gpu::SharedImageInterface* shared_image_interface =
        context_provider_->SharedImageInterface();
    DCHECK(shared_image_interface);

    for (const auto& mb : mailboxes_) {
      if (mb.IsZero() || !mb.IsSharedImage())
        continue;
      shared_image_interface->DestroySharedImage(release_sync_token_, mb);
    }
  }

  // BufferHandleHolder:
  scoped_refptr<media::VideoFrame> OnFrameReadyInBuffer(
      video_capture::mojom::ReadyFrameInBufferPtr buffer) override {
    if (!context_provider_) {
      LOG(ERROR) << "GPU context lost.";
      return {};
    }

    const auto& frame_info = buffer->frame_info;
    if (!MaybeCreateSharedImages(frame_info)) {
      LOG(ERROR) << "Failed to initialize GpuMemoryBufferHandleHolder.";
      return {};
    }

    return WrapMailboxesInVideoFrame(frame_info);
  }

  // viz::ContextLostObserver:
  void OnContextLost() override {
    DCHECK(context_provider_);
    context_provider_->RemoveObserver(this);

    // Clear the mailboxes so that we can recreate the shared images.
    should_create_shared_images_ = true;
    for (auto& mb : mailboxes_)
      mb.SetZero();
    release_sync_token_ = gpu::SyncToken();

    context_provider_ = GetContextFactory()->SharedMainThreadContextProvider();
    if (context_provider_) {
      context_provider_->AddObserver(this);
      buffer_texture_target_ = CalculateBufferTextureTarget(
          context_provider_->ContextCapabilities());
    }
  }

 private:
  // Creates and returns a new `GpuMemoryBuffer` from a cloned handle of our
  // `gpu_memory_buffer_handle_`. The type of the buffer depends on the type of
  // the handle and can only be either `NATIVE_PIXMAP` or
  // `SHARED_MEMORY_BUFFER`.
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBufferFromHandle(
      const gfx::Size& size) {
    const auto buffer_format = GetBufferFormat();
    const auto buffer_usage = GetBufferUsage();
    if (gpu_memory_buffer_handle_.type == gfx::NATIVE_PIXMAP) {
      return gpu::GpuMemoryBufferImplNativePixmap::CreateFromHandle(
          client_native_pixmap_factory_.get(),
          gpu_memory_buffer_handle_.Clone(), size, buffer_format, buffer_usage,
          base::DoNothing());
    }

    DCHECK_EQ(gpu_memory_buffer_handle_.type, gfx::SHARED_MEMORY_BUFFER);
    DCHECK(g_force_use_gpu_memory_buffer_for_test);
    return gpu::GpuMemoryBufferImplSharedMemory::CreateFromHandle(
        gpu_memory_buffer_handle_.Clone(), size, buffer_format, buffer_usage,
        base::DoNothing());
  }

  // Initializes this holder by creating shared images and storing them in
  // `mailboxes_`. These shared images are backed by a GpuMemoryBuffer whose
  // handle is a clone of our `gpu_memory_buffer_handle_`. This operation should
  // only be done the first ever time, or whenever the gpu context is lost.
  // Returns true if shared images are already created or creation is
  // successful. False otherwise.
  bool MaybeCreateSharedImages(
      const media::mojom::VideoFrameInfoPtr& frame_info) {
    DCHECK(context_provider_);

    if (!should_create_shared_images_)
      return true;

    // We clone our handle `gpu_memory_buffer_handle_` and use the cloned handle
    // to create a new GpuMemoryBuffer which will be used to create the shared
    // images. This way, the lifetime of our `gpu_memory_buffer_handle_` remains
    // tied to the lieftime of this object (i.e. until `OnBufferRetired()` is
    // called).
    std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
        CreateGpuMemoryBufferFromHandle(frame_info->coded_size);

    if (!gmb) {
      LOG(ERROR) << "Failed to create a GpuMemoryBuffer.";
      return false;
    }

    gpu::SharedImageInterface* shared_image_interface =
        context_provider_->SharedImageInterface();
    DCHECK(shared_image_interface);

    gpu::GpuMemoryBufferManager* gmb_manager =
        GetContextFactory()->GetGpuMemoryBufferManager();
    for (size_t plane = 0; plane < buffer_planes_.size(); ++plane) {
      mailboxes_[plane] = shared_image_interface->CreateSharedImage(
          gmb.get(), gmb_manager, buffer_planes_[plane],
          frame_info->color_space.value_or(gfx::ColorSpace()),
          kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, kSharedImageUsage);
    }

    // Since this is the first time we create the shared images in `mailboxes_`,
    // we need to guarantee that the mailboxes are created before they're used.
    mailbox_holder_sync_token_ = shared_image_interface->GenVerifiedSyncToken();

    should_create_shared_images_ = false;
    return true;
  }

  // Wraps the shared images in `mailboxes_` in a video frame and returns it if
  // wrapping was successful, or an empty refptr otherwise.
  scoped_refptr<media::VideoFrame> WrapMailboxesInVideoFrame(
      const media::mojom::VideoFrameInfoPtr& frame_info) {
    DCHECK(!should_create_shared_images_);

    if (frame_info->pixel_format !=
            media::VideoPixelFormat::PIXEL_FORMAT_NV12 &&
        frame_info->pixel_format !=
            media::VideoPixelFormat::PIXEL_FORMAT_ARGB) {
      LOG(ERROR) << "Unsupported pixel format";
      return {};
    }

    // The camera GpuMemoryBuffer is backed by a DMA-buff, and doesn't use a
    // pre-mapped shared memory region.
    DCHECK(!frame_info->is_premapped);

    gpu::MailboxHolder mailbox_holder_array[media::VideoFrame::kMaxPlanes];
    for (size_t plane = 0; plane < buffer_planes_.size(); ++plane) {
      DCHECK(!mailboxes_[plane].IsZero());
      DCHECK(mailboxes_[plane].IsSharedImage());
      mailbox_holder_array[plane] =
          gpu::MailboxHolder(mailboxes_[plane], mailbox_holder_sync_token_,
                             buffer_texture_target_);
    }
    mailbox_holder_sync_token_.Clear();

    auto frame = media::VideoFrame::WrapNativeTextures(
        frame_info->pixel_format, mailbox_holder_array,
        base::BindOnce(&GpuMemoryBufferHandleHolder::OnMailboxReleased,
                       weak_ptr_factory_.GetWeakPtr()),
        frame_info->coded_size, frame_info->visible_rect,
        frame_info->visible_rect.size(), frame_info->timestamp);

    if (!frame) {
      LOG(ERROR) << "Failed to create a video frame.";
      return frame;
    }

    if (frame_info->color_space.has_value() &&
        frame_info->color_space->IsValid()) {
      frame->set_color_space(frame_info->color_space.value());
    }
    frame->metadata().allow_overlay = true;
    frame->metadata().read_lock_fences_enabled = true;
    frame->metadata().MergeMetadataFrom(frame_info->metadata);

    return frame;
  }

  // Called when the video frame is destroyed.
  void OnMailboxReleased(const gpu::SyncToken& release_sync_token) {
    release_sync_token_ = release_sync_token;
  }

  // The held GPU buffer handle associated with this object.
  const gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle_;

  // The buffer planes for each we need to create a shared image and store it in
  // `mailboxes_`.
  const std::vector<gfx::BufferPlane> buffer_planes_;

  // Used to create a GPU memory buffer from its handle.
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;

  scoped_refptr<viz::ContextProvider> context_provider_;

  // The texture target we use to create a `MailboxHolder`. This value is
  // calculated for out GPU buffer format, and GPU buffer usage, and the current
  // capabilities of `context_provider_`.
  uint32_t buffer_texture_target_;

  // Contains the shared images of the video frame planes created from the GPU
  // memory buffer.
  std::vector<gpu::Mailbox> mailboxes_{media::VideoFrame::kMaxPlanes};

  // The sync token used when creating a `MailboxHolder`. This will be a
  // verified sync token the first time we wrap a video frame around a mailbox.
  gpu::SyncToken mailbox_holder_sync_token_;

  // The release sync token of the above `mailboxes_`.
  gpu::SyncToken release_sync_token_;

  bool should_create_shared_images_ = true;

  base::WeakPtrFactory<GpuMemoryBufferHandleHolder> weak_ptr_factory_{this};
};

}  // namespace

// -----------------------------------------------------------------------------
// BufferHandleHolder:

BufferHandleHolder::~BufferHandleHolder() = default;

// static
std::unique_ptr<BufferHandleHolder> BufferHandleHolder::Create(
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  if (buffer_handle->is_unsafe_shmem_region()) {
    return std::make_unique<SharedMemoryBufferHandleHolder>(
        std::move(buffer_handle));
  }

  DCHECK(buffer_handle->is_gpu_memory_buffer_handle());
  return std::make_unique<GpuMemoryBufferHandleHolder>(
      std::move(buffer_handle));
}

// -----------------------------------------------------------------------------
// CameraVideoFrameHandler:

CameraVideoFrameHandler::CameraVideoFrameHandler(
    Delegate* delegate,
    mojo::Remote<video_capture::mojom::VideoSource> camera_video_source,
    const media::VideoCaptureFormat& capture_format)
    : delegate_(delegate),
      camera_video_source_remote_(std::move(camera_video_source)) {
  DCHECK(delegate_);
  DCHECK(camera_video_source_remote_);

  camera_video_source_remote_.set_disconnect_handler(
      base::BindOnce(&CameraVideoFrameHandler::OnFatalErrorOrDisconnection,
                     base::Unretained(this)));

  media::VideoCaptureParams capture_params;
  capture_params.requested_format = capture_format;
  AdjustParamsForCurrentConfig(&capture_params);

  camera_video_source_remote_->CreatePushSubscription(
      video_frame_handler_receiver_.BindNewPipeAndPassRemote(), capture_params,
      // The Camera app, or some other camera capture operation may already be
      // running with certain settings. We don't want to reopen the camera
      // device with our settings, since our requirements are usually low in
      // terms of frame rate and size. So we'll use whatever settings available
      // if any.
      /*force_reopen_with_new_settings=*/false,
      camera_video_stream_subsciption_remote_.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](video_capture::mojom::CreatePushSubscriptionResultCodePtr
                 result_code,
             const media::VideoCaptureParams& actual_params) {
            if (result_code->is_error_code()) {
              LOG(ERROR) << "Error in creating push subscription: "
                         << static_cast<int>(result_code->get_error_code());
            }
          }));
}

CameraVideoFrameHandler::~CameraVideoFrameHandler() = default;

void CameraVideoFrameHandler::StartHandlingFrames() {
  DCHECK(camera_video_stream_subsciption_remote_);
  camera_video_stream_subsciption_remote_->Activate();
}

void CameraVideoFrameHandler::OnCaptureConfigurationChanged() {}

void CameraVideoFrameHandler::OnNewBuffer(
    int buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  const auto pair = buffer_map_.emplace(
      buffer_id, BufferHandleHolder::Create(std::move(buffer_handle)));
  DCHECK(pair.second);
}

void CameraVideoFrameHandler::OnFrameAccessHandlerReady(
    mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
        pending_frame_access_handler) {
  video_frame_access_handler_remote_.Bind(
      std::move(pending_frame_access_handler));
}

void CameraVideoFrameHandler::OnFrameReadyInBuffer(
    video_capture::mojom::ReadyFrameInBufferPtr buffer,
    std::vector<video_capture::mojom::ReadyFrameInBufferPtr> scaled_buffers) {
  DCHECK(video_frame_access_handler_remote_);

  // Ignore scaled buffers for now.
  for (auto& scaled_buffer : scaled_buffers) {
    video_frame_access_handler_remote_->OnFinishedConsumingBuffer(
        scaled_buffer->buffer_id);
  }
  scaled_buffers.clear();

  const int buffer_id = buffer->buffer_id;
  const auto& iter = buffer_map_.find(buffer_id);
  DCHECK(iter != buffer_map_.end());

  const auto& buffer_handle_holder = iter->second;
  scoped_refptr<media::VideoFrame> frame =
      buffer_handle_holder->OnFrameReadyInBuffer(std::move(buffer));
  if (!frame) {
    video_frame_access_handler_remote_->OnFinishedConsumingBuffer(buffer_id);
    return;
  }

  frame->AddDestructionObserver(
      base::BindOnce(&CameraVideoFrameHandler::OnVideoFrameGone,
                     weak_ptr_factory_.GetWeakPtr(), buffer_id));

  delegate_->OnCameraVideoFrame(std::move(frame));
}

void CameraVideoFrameHandler::OnBufferRetired(int buffer_id) {
  DCHECK(buffer_map_.contains(buffer_id));
  buffer_map_.erase(buffer_id);
}

void CameraVideoFrameHandler::OnError(media::VideoCaptureError error) {
  LOG(ERROR) << "Recieved error: " << static_cast<int>(error);
  if (IsFatalError(error))
    OnFatalErrorOrDisconnection();
}

void CameraVideoFrameHandler::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  DLOG(ERROR) << "A camera video frame was dropped due to: "
              << static_cast<int>(reason);
}

void CameraVideoFrameHandler::OnNewCropVersion(uint32_t crop_version) {}

void CameraVideoFrameHandler::OnFrameWithEmptyRegionCapture() {}

void CameraVideoFrameHandler::OnLog(const std::string& message) {
  DVLOG(1) << message;
}

void CameraVideoFrameHandler::OnStarted() {}

void CameraVideoFrameHandler::OnStartedUsingGpuDecode() {}

void CameraVideoFrameHandler::OnStopped() {}

// static
void CameraVideoFrameHandler::SetForceUseGpuMemoryBufferForTest(bool value) {
  g_force_use_gpu_memory_buffer_for_test = value;
}

void CameraVideoFrameHandler::OnVideoFrameGone(int buffer_id) {
  DCHECK(video_frame_access_handler_remote_);
  video_frame_access_handler_remote_->OnFinishedConsumingBuffer(buffer_id);
}

void CameraVideoFrameHandler::OnFatalErrorOrDisconnection() {
  buffer_map_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();
  video_frame_handler_receiver_.reset();
  camera_video_source_remote_.reset();
  camera_video_stream_subsciption_remote_.reset();
  video_frame_access_handler_remote_.reset();

  CaptureModeController::Get()->camera_controller()->OnFrameHandlerFatalError();
  // `this` will be deleted soon after the above call. "Soon" here because the
  // `camera_preview_widget_` which indirectly owns `this` is destroyed
  // asynchronously when `Close()` is called on it.
}

}  // namespace ash
