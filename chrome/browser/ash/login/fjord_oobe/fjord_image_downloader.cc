// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/fjord_oobe/fjord_image_downloader.h"

#include <utility>

#include "base/logging.h"
#include "chromeos/dbus/dissidia/dissidia_client.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

namespace {

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
    base::OnceCallback<void(bool success)> callback) {
  if (is_running_) {
    LOG(WARNING) << "Dissidia is already running, ignoring new request";
    std::move(callback).Run(false);
    return;
  }

  chromeos::DissidiaClient* client = chromeos::DissidiaClient::Get();
  if (!client) {
    LOG(ERROR) << "DissidiaClient is not available";
    std::move(callback).Run(false);
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
    is_running_ = false;
    if (callback_) {
      std::move(callback_).Run(false);
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
    std::move(callback_).Run(success);
  }
}

}  // namespace ash
