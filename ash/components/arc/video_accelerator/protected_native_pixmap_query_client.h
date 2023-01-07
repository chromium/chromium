// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_NATIVE_PIXMAP_QUERY_CLIENT_H_
#define ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_NATIVE_PIXMAP_QUERY_CLIENT_H_

#include "ash/components/arc/mojom/protected_buffer_manager.mojom.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/protected_native_pixmap_query_delegate.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

// Implementation class for querying the GPU process for whether a handle maps
// to a Chrome allocated protected buffer.
class ProtectedNativePixmapQueryClient
    : public exo::ProtectedNativePixmapQueryDelegate {
 public:
  ProtectedNativePixmapQueryClient();
  ProtectedNativePixmapQueryClient(const ProtectedNativePixmapQueryClient&) =
      delete;
  ProtectedNativePixmapQueryClient& operator=(
      const ProtectedNativePixmapQueryClient&) = delete;
  ~ProtectedNativePixmapQueryClient() override;

  // exo::ProtectedNativePixmapQueryDelegate implementation.
  void IsProtectedNativePixmapHandle(
      base::ScopedFD handle,
      IsProtectedNativePixmapHandleCallback callback) override;

 private:
  void OnMojoDisconnect();
  mojo::Remote<mojom::ProtectedBufferManager> gpu_buffer_manager_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ProtectedNativePixmapQueryClient> weak_factory_{this};
};
}  // namespace arc
#endif  // ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_NATIVE_PIXMAP_QUERY_CLIENT_H_
