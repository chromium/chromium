// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_CLIENT_H_
#define ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_CLIENT_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/services/machine_learning/public/mojom/document_scanner.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

// Client for communicating to the CrOS Document Scanner Service.
class DocumentScannerServiceClient {
 public:
  using OnReadyCallback = base::OnceCallback<void(bool is_supported)>;
  using DetectCornersCallback =
      base::OnceCallback<void(bool success,
                              const std::vector<gfx::PointF>& results)>;
  using DoPostProcessingCallback = base::OnceCallback<
      void(bool success, const std::vector<uint8_t>& processed_jpeg_image)>;

  static bool IsSupported();

  static bool IsSupportedByDlc();

  static std::unique_ptr<DocumentScannerServiceClient> Create();

  ~DocumentScannerServiceClient();

  void CheckDocumentModeReadiness(OnReadyCallback callback);

  bool IsLoaded();

  void DetectCornersFromNV12Image(base::ReadOnlySharedMemoryRegion nv12_image,
                                  DetectCornersCallback callback);

  void DetectCornersFromJPEGImage(base::ReadOnlySharedMemoryRegion jpeg_image,
                                  DetectCornersCallback callback);

  void DoPostProcessing(base::ReadOnlySharedMemoryRegion jpeg_image,
                        const std::vector<gfx::PointF>& corners,
                        chromeos::machine_learning::mojom::Rotation rotation,
                        DoPostProcessingCallback callback);

 protected:
  DocumentScannerServiceClient();

 private:
  void LoadDocumentScanner();

  void LoadDocumentScannerInternal(const std::string& lib_path);

  void OnLoadedDocumentScanner(
      chromeos::machine_learning::mojom::LoadModelResult result);

  // Guards |document_scanner_loaded_| and |on_ready_callbacks_| which are
  // related to the load status.
  base::Lock load_status_lock_;

  bool is_loading_ GUARDED_BY(load_status_lock_) = false;

  bool document_scanner_loaded_ GUARDED_BY(load_status_lock_) = false;

  std::vector<OnReadyCallback> on_ready_callbacks_
      GUARDED_BY(load_status_lock_);

  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>
      ml_service_;

  mojo::Remote<chromeos::machine_learning::mojom::DocumentScanner>
      document_scanner_;

  base::WeakPtrFactory<DocumentScannerServiceClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_CLIENT_H_
