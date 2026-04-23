// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/fjord_oobe/fjord_image_downloader.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/dissidia/dissidia_client.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

namespace {

constexpr char kResultHistogramName[] = "OOBE.FjordImageDownloader.Result";

std::string GetImageName(FjordImageDownloader::ImageType image_type) {
  switch (image_type) {
    case FjordImageDownloader::ImageType::kNoctis:
      return "noctis";
    case FjordImageDownloader::ImageType::kSelphie:
      return "selphie";
    case FjordImageDownloader::ImageType::kUnknown:
      NOTREACHED();
  }
}

FjordImageDownloader::ImageDownloadResult PerformUpdateStatusToResult(
    dissidia::PerformUpdateStatus status) {
  switch (status) {
    case dissidia::PerformUpdateStatus::kAlreadyOnRequestedImage:
      return FjordImageDownloader::ImageDownloadResult::
          kAlreadyOnRequestedImage;
    case dissidia::PerformUpdateStatus::kUpdateAlreadyInProgress:
      return FjordImageDownloader::ImageDownloadResult::
          kUpdateAlreadyInProgress;
    case dissidia::PerformUpdateStatus::kOobeAlreadyCompleted:
      return FjordImageDownloader::ImageDownloadResult::kOobeAlreadyCompleted;
    case dissidia::PerformUpdateStatus::kError:
      return FjordImageDownloader::ImageDownloadResult::kPerformUpdateError;
    case dissidia::PerformUpdateStatus::kUpdateStarted:
      NOTREACHED();
  }
}

FjordImageDownloader::ImageDownloadResult CompletedErrorCodeToResult(
    dissidia::CompletedErrorCode error_code) {
  switch (error_code) {
    case dissidia::CompletedErrorCode::kSuccess:
      return FjordImageDownloader::ImageDownloadResult::kSuccess;
    case dissidia::CompletedErrorCode::kGeneralFailure:
      return FjordImageDownloader::ImageDownloadResult::kGeneralFailure;
    case dissidia::CompletedErrorCode::kDownloadFailed:
      return FjordImageDownloader::ImageDownloadResult::kDownloadFailed;
    case dissidia::CompletedErrorCode::kSlotResolutionFailed:
      return FjordImageDownloader::ImageDownloadResult::kSlotResolutionFailed;
    case dissidia::CompletedErrorCode::kExtractFailed:
      return FjordImageDownloader::ImageDownloadResult::kExtractFailed;
    case dissidia::CompletedErrorCode::kRootdevFailed:
      return FjordImageDownloader::ImageDownloadResult::kRootdevFailed;
    case dissidia::CompletedErrorCode::kCgptFailed:
      return FjordImageDownloader::ImageDownloadResult::kCgptFailed;
    case dissidia::CompletedErrorCode::kRebootFailed:
      return FjordImageDownloader::ImageDownloadResult::kRebootFailed;
  }
}

}  // namespace

FjordImageDownloader::FjordImageDownloader() {
  chromeos::DissidiaClient* client = chromeos::DissidiaClient::Get();
  if (client) {
    client->AddObserver(this);
  }
}

FjordImageDownloader::~FjordImageDownloader() {
  chromeos::DissidiaClient* client = chromeos::DissidiaClient::Get();
  if (client) {
    client->RemoveObserver(this);
  }
}

void FjordImageDownloader::RunDissidia(
    ImageType image_type,
    base::OnceCallback<void(bool success, const std::string& error_message)>
        callback) {
  if (is_running_) {
    LOG(WARNING) << "Dissidia is already running, ignoring new request";
    std::move(callback).Run(false, "Update already in progress");
    return;
  }

  chromeos::DissidiaClient* client = chromeos::DissidiaClient::Get();
  if (!client) {
    LOG(ERROR) << "DissidiaClient is not available";
    std::move(callback).Run(false, "Client is not available");
    return;
  }

  is_running_ = true;
  callback_ = std::move(callback);
  std::string image_name = GetImageName(image_type);

  client->PerformUpdate(
      image_name, base::BindOnce(&FjordImageDownloader::OnPerformUpdateResponse,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void FjordImageDownloader::OnPerformUpdateResponse(
    dissidia::PerformUpdateStatus status,
    const std::string& message) {
  if (status != dissidia::PerformUpdateStatus::kUpdateStarted) {
    LOG(ERROR) << "PerformUpdate failed with status "
               << static_cast<int>(status) << ": " << message;
    base::UmaHistogramEnumeration(kResultHistogramName,
                                  PerformUpdateStatusToResult(status));
    is_running_ = false;
    if (callback_) {
      std::move(callback_).Run(
          false, base::StringPrintf("Error %d: %s", static_cast<int>(status),
                                    message.c_str()));
    }
  }
}

void FjordImageDownloader::OnProgress(int32_t percent,
                                      const std::string& stage) {
  VLOG(1) << "FjordImageDownloader::OnProgress percent=" << percent
          << ", stage=" << stage;
}

void FjordImageDownloader::OnCompleted(bool success,
                                       dissidia::CompletedErrorCode error_code,
                                       const std::string& message) {
  is_running_ = false;
  base::UmaHistogramEnumeration(kResultHistogramName,
                                CompletedErrorCodeToResult(error_code));

  if (success) {
    VLOG(1) << "Dissidia completed successfully: " << message;
    chromeos::PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_OTHER,
        "Fjord image download completed, rebooting device");
  } else {
    LOG(ERROR) << "Dissidia failed with error code "
               << static_cast<int>(error_code) << ": " << message;
  }

  if (callback_) {
    std::move(callback_).Run(
        success,
        base::StringPrintf("Error %d: %s", static_cast<int>(error_code),
                           message.c_str()));
  }
}

}  // namespace ash
