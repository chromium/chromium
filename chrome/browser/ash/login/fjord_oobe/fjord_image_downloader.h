// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_IMAGE_DOWNLOADER_H_
#define CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_IMAGE_DOWNLOADER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/dissidia/dissidia_client.h"
#include "third_party/cros_system_api/dbus/dissidia/dbus-constants.h"

namespace ash {

// Uses the dissidia-daemon D-Bus service to perform image switching on Fjord
// devices. Calls PerformUpdate and listens for Progress/Completed signals.
class FjordImageDownloader : public chromeos::DissidiaClient::Observer {
 public:
  // The image type to download.
  enum class ImageType {
    kUnknown,
    kNoctis,
    kSelphie,
  };

  // LINT.IfChange(ImageDownloadResult)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ImageDownloadResult {
    kSuccess = 0,                  // Completed successfully
    kAlreadyOnRequestedImage = 1,  // PerformUpdate: already on image
    kUpdateAlreadyInProgress = 2,  // PerformUpdate: update in progress
    kOobeAlreadyCompleted = 3,     // PerformUpdate: OOBE completed
    kPerformUpdateError = 4,       // PerformUpdate: generic error
    kGeneralFailure = 5,           // Completed: general failure
    kDownloadFailed = 6,           // Completed: download failed
    kSlotResolutionFailed = 7,     // Completed: slot resolution failed
    kExtractFailed = 8,            // Completed: extract failed
    kRootdevFailed = 9,            // Completed: rootdev failed
    kCgptFailed = 10,              // Completed: cgpt failed
    kRebootFailed = 11,            // Completed: reboot failed
    kMaxValue = kRebootFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/enums.xml:FjordImageDownloadResult)

  FjordImageDownloader();
  FjordImageDownloader(const FjordImageDownloader&) = delete;
  FjordImageDownloader& operator=(const FjordImageDownloader&) = delete;
  ~FjordImageDownloader() override;

  // Initiates an image update via the dissidia D-Bus service. The
  // |image_type| determines which image name to request. The result is
  // reported via |callback| when the Completed signal is received.
  void RunDissidia(
      ImageType image_type,
      base::OnceCallback<void(bool success, const std::string& error_message)>
          callback);

  // chromeos::DissidiaClient::Observer:
  void OnProgress(int32_t percent, const std::string& stage) override;
  void OnCompleted(bool success,
                   dissidia::CompletedErrorCode error_code,
                   const std::string& message) override;

 private:
  // Called when the PerformUpdate D-Bus method returns.
  void OnPerformUpdateResponse(dissidia::PerformUpdateStatus status,
                               const std::string& message);

  base::OnceCallback<void(bool success, const std::string& error_message)>
      callback_;
  bool is_running_ = false;

  base::WeakPtrFactory<FjordImageDownloader> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_IMAGE_DOWNLOADER_H_
