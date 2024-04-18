// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_HOST_H_
#define ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_HOST_H_

#include "ash/webui/camera_app_ui/document_scanner_service_client.h"
#include "base/memory/singleton.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "media/capture/video/chromeos/mojom/document_scanner.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

// A host for ChromeOS VCD to request document scanning service to detect
// corners.
class DocumentScannerServiceHost
    : public cros::mojom::CrosDocumentScanner,
      public chromeos::mojo_service_manager::mojom::ServiceProvider {
 public:
  DocumentScannerServiceHost(const DocumentScannerServiceHost&) = delete;
  DocumentScannerServiceHost& operator=(const DocumentScannerServiceHost&) =
      delete;

  // TODO(b/333927344): Find an owner to avoid Singleton pattern.
  // MediaStreamManager is not a good candidate since it initializes before ML
  // service.
  static DocumentScannerServiceHost* GetInstance();

  void Start();

  void DetectCornersFromNV12Image(
      base::ReadOnlySharedMemoryRegion nv12_image,
      DetectCornersFromNV12ImageCallback callback) override;

  // chromeos::mojo_service_manager::mojom::ServiceProvider overrides.
  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
      mojo::ScopedMessagePipeHandle receiver) override;

 private:
  friend struct base::DefaultSingletonTraits<DocumentScannerServiceHost>;

  DocumentScannerServiceHost();

  ~DocumentScannerServiceHost() override;

  std::unique_ptr<DocumentScannerServiceClient> document_scanner_service_;

  mojo::ReceiverSet<cros::mojom::CrosDocumentScanner> receiver_set_;

  // Receiver for mojo service manager service provider.
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DocumentScannerServiceHost> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_HOST_H_
