// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/video_accelerator/protected_native_pixmap_query_client.h"
#include "content/public/browser/gpu_service_registry.h"

namespace arc {

ProtectedNativePixmapQueryClient::ProtectedNativePixmapQueryClient() {}

ProtectedNativePixmapQueryClient::~ProtectedNativePixmapQueryClient() {}

void ProtectedNativePixmapQueryClient::IsProtectedNativePixmapHandle(
    base::ScopedFD handle,
    IsProtectedNativePixmapHandleCallback callback) {
  if (!gpu_buffer_manager_) {
    content::BindInterfaceInGpuProcess(
        gpu_buffer_manager_.BindNewPipeAndPassReceiver());
    gpu_buffer_manager_.set_disconnect_handler(
        base::BindOnce(&ProtectedNativePixmapQueryClient::OnMojoDisconnect,
                       weak_factory_.GetWeakPtr()));
  }
  gpu_buffer_manager_->IsProtectedNativePixmapHandle(
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(handle))),
      std::move(callback));
}

void ProtectedNativePixmapQueryClient::OnMojoDisconnect() {
  gpu_buffer_manager_.reset();
}

}  // namespace arc
