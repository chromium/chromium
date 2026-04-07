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

  FjordImageDownloader();
  FjordImageDownloader(const FjordImageDownloader&) = delete;
  FjordImageDownloader& operator=(const FjordImageDownloader&) = delete;
  ~FjordImageDownloader() override;

  // Initiates an image update via the dissidia D-Bus service. The
  // |image_type| determines which image name to request. The result is
  // reported via |callback| when the Completed signal is received.
  void RunDissidia(ImageType image_type,
                   base::OnceCallback<void(bool success)> callback);

  // chromeos::DissidiaClient::Observer:
  void OnProgress(int32_t percent, const std::string& stage) override;
  void OnCompleted(bool success,
                   dissidia::CompletedErrorCode error_code,
                   const std::string& message) override;

 private:
  // Called when the PerformUpdate D-Bus method returns.
  void OnPerformUpdateResponse(dissidia::PerformUpdateStatus status,
                               const std::string& message);

  base::OnceCallback<void(bool success)> callback_;
  bool is_running_ = false;

  base::WeakPtrFactory<FjordImageDownloader> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_IMAGE_DOWNLOADER_H_
