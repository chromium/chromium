// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/document_scanner_service_client.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
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

bool IsMachineLearningServiceAvailable() {
  return HasCommandLineSwitch(kMLService, "enabled");
}

}  // namespace

// static
bool DocumentScannerServiceClient::IsSupported() {
  return IsMachineLearningServiceAvailable() && IsEnabledOnRootfs();
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

bool DocumentScannerServiceClient::IsLoaded() {
  return document_scanner_loaded_;
}

void DocumentScannerServiceClient::DetectCornersFromNV12Image(
    base::ReadOnlySharedMemoryRegion nv12_image,
    DetectCornersCallback callback) {
  DCHECK(IsSupported());

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
  DCHECK(IsSupported());

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
  DCHECK(IsSupported());

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
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->BindMachineLearningService(ml_service_.BindNewPipeAndPassReceiver());
  ml_service_->LoadDocumentScanner(
      document_scanner_.BindNewPipeAndPassReceiver(),
      base::BindOnce(&DocumentScannerServiceClient::OnInitialized,
                     base::Unretained(this)));
}

void DocumentScannerServiceClient::OnInitialized(
    chromeos::machine_learning::mojom::LoadModelResult result) {
  document_scanner_loaded_ = result == LoadModelResult::OK;
}

}  // namespace ash
