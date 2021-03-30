// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/video_frame_handler_ash.h"

#include <memory>
#include <string>
#include <utility>

#include "base/notreached.h"
#include "media/base/video_transformation.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/buffer.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace crosapi {

namespace {

crosapi::mojom::ReadyFrameInBufferPtr ToCrosapiBuffer(
    video_capture::mojom::ReadyFrameInBufferPtr buffer) {
  auto crosapi_buffer = crosapi::mojom::ReadyFrameInBuffer::New();
  crosapi_buffer->buffer_id = buffer->buffer_id;
  crosapi_buffer->frame_feedback_id = buffer->frame_feedback_id;

  mojo::PendingRemote<crosapi::mojom::ScopedAccessPermission> access_permission;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<VideoFrameHandlerAsh::AccessPermissionProxy>(
          std::move(buffer->access_permission)),
      access_permission.InitWithNewPipeAndPassReceiver());
  crosapi_buffer->access_permission = std::move(access_permission);

  const auto& buffer_info = buffer->frame_info;
  auto crosapi_buffer_info = crosapi::mojom::VideoFrameInfo::New();
  crosapi_buffer_info->timestamp = buffer_info->timestamp;
  crosapi_buffer_info->pixel_format = buffer_info->pixel_format;
  crosapi_buffer_info->coded_size = buffer_info->coded_size;
  crosapi_buffer_info->visible_rect = buffer_info->visible_rect;

  auto transformation = buffer_info->metadata.transformation;
  if (transformation) {
    crosapi::mojom::VideoRotation crosapi_rotation;
    switch (transformation->rotation) {
      case media::VideoRotation::VIDEO_ROTATION_0:
        crosapi_rotation = crosapi::mojom::VideoRotation::kVideoRotation0;
        break;
      case media::VideoRotation::VIDEO_ROTATION_90:
        crosapi_rotation = crosapi::mojom::VideoRotation::kVideoRotation90;
        break;
      case media::VideoRotation::VIDEO_ROTATION_180:
        crosapi_rotation = crosapi::mojom::VideoRotation::kVideoRotation180;
        break;
      case media::VideoRotation::VIDEO_ROTATION_270:
        crosapi_rotation = crosapi::mojom::VideoRotation::kVideoRotation270;
        break;
      default:
        NOTREACHED() << "Unexpected rotation in video frame metadata";
    }
    crosapi_buffer_info->rotation = crosapi_rotation;
  }
  if (buffer_info->metadata.reference_time.has_value())
    crosapi_buffer_info->reference_time = *buffer_info->metadata.reference_time;

  crosapi_buffer->frame_info = std::move(crosapi_buffer_info);
  return crosapi_buffer;
}

crosapi::mojom::GpuMemoryBufferHandlePtr ToCrosapiGpuMemoryBufferHandle(
    gfx::GpuMemoryBufferHandle buffer_handle) {
  auto crosapi_gpu_handle = crosapi::mojom::GpuMemoryBufferHandle::New();
  crosapi_gpu_handle->id = buffer_handle.id.id;
  crosapi_gpu_handle->offset = buffer_handle.offset;
  crosapi_gpu_handle->stride = buffer_handle.stride;

  if (buffer_handle.type == gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER) {
    auto crosapi_platform_handle =
        crosapi::mojom::GpuMemoryBufferPlatformHandle::New();
    crosapi_platform_handle->set_shared_memory_handle(
        std::move(buffer_handle.region));
    crosapi_gpu_handle->platform_handle = std::move(crosapi_platform_handle);
  } else if (buffer_handle.type == gfx::GpuMemoryBufferType::NATIVE_PIXMAP) {
    auto crosapi_platform_handle =
        crosapi::mojom::GpuMemoryBufferPlatformHandle::New();
    auto crosapi_native_pixmap_handle =
        crosapi::mojom::NativePixmapHandle::New();
    crosapi_native_pixmap_handle->planes =
        std::move(buffer_handle.native_pixmap_handle.planes);
    crosapi_native_pixmap_handle->modifier =
        buffer_handle.native_pixmap_handle.modifier;
    crosapi_platform_handle->set_native_pixmap_handle(
        std::move(crosapi_native_pixmap_handle));
    crosapi_gpu_handle->platform_handle = std::move(crosapi_platform_handle);
  }
  return crosapi_gpu_handle;
}

}  // namespace

VideoFrameHandlerAsh::VideoFrameHandlerAsh(
    mojo::PendingReceiver<video_capture::mojom::VideoFrameHandler>
        handler_receiver,
    mojo::PendingRemote<crosapi::mojom::VideoFrameHandler> proxy_remote)
    : proxy_(std::move(proxy_remote)) {
  receiver_.Bind(std::move(handler_receiver));
}

VideoFrameHandlerAsh::~VideoFrameHandlerAsh() = default;

VideoFrameHandlerAsh::AccessPermissionProxy::AccessPermissionProxy(
    mojo::PendingRemote<video_capture::mojom::ScopedAccessPermission> remote)
    : remote_(std::move(remote)) {}

VideoFrameHandlerAsh::AccessPermissionProxy::~AccessPermissionProxy() = default;

void VideoFrameHandlerAsh::OnNewBuffer(
    int buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  crosapi::mojom::VideoBufferHandlePtr crosapi_handle =
      crosapi::mojom::VideoBufferHandle::New();

  if (buffer_handle->is_shared_buffer_handle()) {
    crosapi_handle->set_shared_buffer_handle(
        buffer_handle->get_shared_buffer_handle()->Clone(
            mojo::SharedBufferHandle::AccessMode::READ_WRITE));
  } else if (buffer_handle->is_gpu_memory_buffer_handle()) {
    crosapi_handle->set_gpu_memory_buffer_handle(ToCrosapiGpuMemoryBufferHandle(
        std::move(buffer_handle->get_gpu_memory_buffer_handle())));
  } else {
    NOTREACHED() << "Unexpected new buffer type";
  }
  proxy_->OnNewBuffer(buffer_id, std::move(crosapi_handle));
}

void VideoFrameHandlerAsh::OnFrameReadyInBuffer(
    video_capture::mojom::ReadyFrameInBufferPtr buffer,
    std::vector<video_capture::mojom::ReadyFrameInBufferPtr> scaled_buffers) {
  crosapi::mojom::ReadyFrameInBufferPtr crosapi_buffer =
      ToCrosapiBuffer(std::move(buffer));
  std::vector<crosapi::mojom::ReadyFrameInBufferPtr> crosapi_scaled_buffers;
  for (auto& b : scaled_buffers)
    crosapi_scaled_buffers.push_back(ToCrosapiBuffer(std::move(b)));

  proxy_->OnFrameReadyInBuffer(std::move(crosapi_buffer),
                               std::move(crosapi_scaled_buffers));
}

void VideoFrameHandlerAsh::OnBufferRetired(int buffer_id) {
  proxy_->OnBufferRetired(buffer_id);
}

void VideoFrameHandlerAsh::OnError(media::VideoCaptureError error) {
  proxy_->OnError(error);
}

void VideoFrameHandlerAsh::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  proxy_->OnFrameDropped(reason);
}

void VideoFrameHandlerAsh::OnLog(const std::string& message) {
  proxy_->OnLog(message);
}

void VideoFrameHandlerAsh::OnStarted() {
  proxy_->OnStarted();
}

void VideoFrameHandlerAsh::OnStartedUsingGpuDecode() {
  proxy_->OnStartedUsingGpuDecode();
}

void VideoFrameHandlerAsh::OnStopped() {
  proxy_->OnStopped();
}

}  // namespace crosapi
