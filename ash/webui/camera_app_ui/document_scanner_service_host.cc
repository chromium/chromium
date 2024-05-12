// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/document_scanner_service_host.h"

#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash {

DocumentScannerServiceHost::DocumentScannerServiceHost() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DocumentScannerServiceHost::~DocumentScannerServiceHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
DocumentScannerServiceHost* DocumentScannerServiceHost::GetInstance() {
  return base::Singleton<DocumentScannerServiceHost>::get();
}

void DocumentScannerServiceHost::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (document_scanner_service_) {
    return;
  }
  document_scanner_service_ = DocumentScannerServiceClient::Create();
  if (ash::mojo_service_manager::IsServiceManagerBound()) {
    auto* proxy = ash::mojo_service_manager::GetServiceManagerProxy();
    proxy->Register(
        /*service_name=*/chromeos::mojo_services::kCrosDocumentScanner,
        provider_receiver_.BindNewPipeAndPassRemote());
  }
}

void DocumentScannerServiceHost::DetectCornersFromNV12Image(
    base::ReadOnlySharedMemoryRegion nv12_image,
    DetectCornersFromNV12ImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!document_scanner_service_->IsLoaded()) {
    auto detect_result = cros::mojom::DetectCornersResult::New();
    detect_result->success = false;
    detect_result->corners = {};
    std::move(callback).Run(std::move(detect_result));
    return;
  }
  document_scanner_service_->DetectCornersFromNV12Image(
      std::move(nv12_image),
      base::BindOnce(
          [](DetectCornersFromNV12ImageCallback callback, bool success,
             const std::vector<gfx::PointF>& corners) {
            auto detect_result = cros::mojom::DetectCornersResult::New();
            detect_result->success = success;
            detect_result->corners = corners;
            std::move(callback).Run(std::move(detect_result));
          },
          std::move(callback)));
}

void DocumentScannerServiceHost::Request(
    chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
    mojo::ScopedMessagePipeHandle receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_set_.Add(this,
                    mojo::PendingReceiver<cros::mojom::CrosDocumentScanner>(
                        std::move(receiver)));
}

}  // namespace ash
