// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/document_scanner_service_client.h"

#include "ash/webui/camera_app_ui/document_scanner_installer.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

namespace {

using chromeos::machine_learning::mojom::DetectCornersResultPtr;
using chromeos::machine_learning::mojom::DocumentScannerResultStatus;
using chromeos::machine_learning::mojom::DoPostProcessingResultPtr;
using chromeos::machine_learning::mojom::LoadModelResult;
using chromeos::machine_learning::mojom::Rotation;

constexpr char kOndeviceDocumentScanner[] = "ondevice_document_scanner";
constexpr char kMLService[] = "ml_service";
constexpr char kLibDocumentScannerDefaultDir[] =
    "/usr/share/cros-camera/libfs/";

// Returns whether the `value` is set for command line switch
// kOndeviceDocumentScanner.
bool HasCommandLineSwitch(const std::string& key, const std::string& value) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(key) &&
         command_line->GetSwitchValueASCII(key) == value;
}

// Returns true if switch kOndeviceDocumentScanner is set to use_rootfs.
bool IsEnabledOnRootfs() {
  return HasCommandLineSwitch(kOndeviceDocumentScanner, "use_rootfs");
}

// Returns true if switch kOndeviceDocumentScanner is set to use_dlc.
bool IsEnabledOnDlc() {
  return HasCommandLineSwitch(kOndeviceDocumentScanner, "use_dlc");
}

bool IsMachineLearningServiceAvailable() {
  return HasCommandLineSwitch(kMLService, "enabled");
}

}  // namespace

// static
bool DocumentScannerServiceClient::IsSupported() {
  return IsMachineLearningServiceAvailable() &&
         (IsEnabledOnRootfs() || IsEnabledOnDlc());
}

// static
bool DocumentScannerServiceClient::IsSupportedByDlc() {
  return IsMachineLearningServiceAvailable() && IsEnabledOnDlc();
}

// static
std::unique_ptr<DocumentScannerServiceClient>
DocumentScannerServiceClient::Create() {
  if (!IsSupported()) {
    CAMERA_LOG(DEBUG) << "Document scanner is not supported on the device";
    return nullptr;
  }
  return base::WrapUnique(new DocumentScannerServiceClient);
}

DocumentScannerServiceClient::~DocumentScannerServiceClient() = default;

void DocumentScannerServiceClient::CheckDocumentModeReadiness(
    OnReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (document_scanner_loaded_ == LoadStatus::LOADED ||
      document_scanner_loaded_ == LoadStatus::LOAD_FAILED) {
    std::move(callback).Run(document_scanner_loaded_ == LoadStatus::LOADED);
    return;
  }
  on_ready_callbacks_.push_back(std::move(callback));
  LoadDocumentScanner();
}

bool DocumentScannerServiceClient::IsLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return document_scanner_loaded_ == LoadStatus::LOADED;
}

void DocumentScannerServiceClient::DetectCornersFromNV12Image(
    base::ReadOnlySharedMemoryRegion nv12_image,
    DetectCornersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsLoaded()) {
    std::move(callback).Run(false, {});
    return;
  }
  document_scanner_->DetectCornersFromNV12Image(
      std::move(nv12_image),
      base::BindOnce(
          [](DetectCornersCallback callback, DetectCornersResultPtr result) {
            std::move(callback).Run(
                result->status == DocumentScannerResultStatus::OK,
                result->corners);
          },
          std::move(callback)));
}

void DocumentScannerServiceClient::DetectCornersFromJPEGImage(
    base::ReadOnlySharedMemoryRegion jpeg_image,
    DetectCornersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsLoaded()) {
    std::move(callback).Run(false, {});
    return;
  }
  document_scanner_->DetectCornersFromJPEGImage(
      std::move(jpeg_image),
      base::BindOnce(
          [](DetectCornersCallback callback, DetectCornersResultPtr result) {
            std::move(callback).Run(
                result->status == DocumentScannerResultStatus::OK,
                result->corners);
          },
          std::move(callback)));
}

void DocumentScannerServiceClient::DoPostProcessing(
    base::ReadOnlySharedMemoryRegion jpeg_image,
    const std::vector<gfx::PointF>& corners,
    Rotation rotation,
    DoPostProcessingCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsLoaded()) {
    std::move(callback).Run(false, {});
    return;
  }
  document_scanner_->DoPostProcessing(
      std::move(jpeg_image), corners, rotation,
      base::BindOnce(
          [](DoPostProcessingCallback callback,
             DoPostProcessingResultPtr result) {
            std::move(callback).Run(
                result->status == DocumentScannerResultStatus::OK,
                result->processed_jpeg_image);
          },
          std::move(callback)));
}

DocumentScannerServiceClient::DocumentScannerServiceClient() {
  LoadDocumentScanner();
}

void DocumentScannerServiceClient::OnMojoDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ml_service_.reset();
  document_scanner_.reset();

  if (document_scanner_loaded_ == LoadStatus::LOAD_FAILED) {
    return;
  }
  document_scanner_loaded_ = LoadStatus::NOT_LOADED;
  LoadDocumentScanner();
}

void DocumentScannerServiceClient::LoadDocumentScanner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (document_scanner_loaded_ != LoadStatus::NOT_LOADED) {
    return;
  }
  document_scanner_loaded_ = LoadStatus::LOADING;

  if (IsEnabledOnRootfs()) {
    LoadDocumentScannerInternal(kLibDocumentScannerDefaultDir);
  } else if (IsEnabledOnDlc()) {
    DocumentScannerInstaller::GetInstance()->RegisterLibraryPathCallback(
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &DocumentScannerServiceClient::LoadDocumentScannerInternal,
            weak_ptr_factory_.GetWeakPtr())));
  } else {
    OnLoadedDocumentScanner(chromeos::machine_learning::mojom::LoadModelResult::
                                FEATURE_NOT_SUPPORTED_ERROR);
  }
}

void DocumentScannerServiceClient::LoadDocumentScannerInternal(
    const std::string& lib_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (lib_path.empty()) {
    OnLoadedDocumentScanner(chromeos::machine_learning::mojom::LoadModelResult::
                                FEATURE_NOT_SUPPORTED_ERROR);
    return;
  }

  auto config = chromeos::machine_learning::mojom::DocumentScannerConfig::New();
  config->library_dlc_path = base::FilePath(lib_path);

  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->BindMachineLearningService(ml_service_.BindNewPipeAndPassReceiver());
  ml_service_->LoadDocumentScanner(
      document_scanner_.BindNewPipeAndPassReceiver(), std::move(config),
      base::BindOnce(&DocumentScannerServiceClient::OnLoadedDocumentScanner,
                     base::Unretained(this)));
  document_scanner_.set_disconnect_handler(
      base::BindOnce(&DocumentScannerServiceClient::OnMojoDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DocumentScannerServiceClient::OnLoadedDocumentScanner(
    chromeos::machine_learning::mojom::LoadModelResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  document_scanner_loaded_ = result == LoadModelResult::OK
                                 ? LoadStatus::LOADED
                                 : LoadStatus::LOAD_FAILED;

  for (auto& callback : on_ready_callbacks_) {
    std::move(callback).Run(document_scanner_loaded_ == LoadStatus::LOADED);
  }
  on_ready_callbacks_.clear();
}

}  // namespace ash
