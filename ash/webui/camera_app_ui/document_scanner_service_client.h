// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_CLIENT_H_
#define ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_CLIENT_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
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
  enum class LoadStatus {
    NOT_LOADED,  // ServiceClient has not tried to connect to the ML process yet
                 // or failed to connect to the ML process and will try to
                 // load the document scanner again.
    LOADING,     // ServiceClient is loading the document scanner.
    LOAD_FAILED,  // ServiceClient connected to the ML process successfully but
                  // failed to load the document scanner from it. In this case,
                  // ServiceClient will not try to reload.
    LOADED        // ServiceClient loaded the document scanner successfully.
  };

  void LoadDocumentScanner();

  void LoadDocumentScannerInternal(const std::string& lib_path);

  void OnLoadedDocumentScanner(
      chromeos::machine_learning::mojom::LoadModelResult result);

  void OnMojoDisconnected();

  DetectCornersCallback* AddDetectCornersCallback(
      DetectCornersCallback callback);

  void ConsumeDetectCornersCallback(
      DetectCornersCallback* callback_id,
      chromeos::machine_learning::mojom::DetectCornersResultPtr result);

  DoPostProcessingCallback* AddDoPostProcessingCallback(
      DoPostProcessingCallback callback);

  void ConsumeDoPostProcessingCallback(
      DoPostProcessingCallback* callback_id,
      chromeos::machine_learning::mojom::DoPostProcessingResultPtr result);

  void CleanupCallbacks();

  LoadStatus document_scanner_loaded_ = LoadStatus::NOT_LOADED;

  std::vector<OnReadyCallback> on_ready_callbacks_;

  std::unordered_map<DetectCornersCallback*,
                     std::unique_ptr<DetectCornersCallback>>
      detect_corners_callbacks_;

  std::unordered_map<DoPostProcessingCallback*,
                     std::unique_ptr<DoPostProcessingCallback>>
      do_post_processing_callbacks_;

  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>
      ml_service_;

  mojo::Remote<chromeos::machine_learning::mojom::DocumentScanner>
      document_scanner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DocumentScannerServiceClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_CLIENT_H_
