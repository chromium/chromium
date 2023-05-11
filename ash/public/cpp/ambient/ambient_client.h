// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_CLIENT_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_CLIENT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/image_downloader.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace base {
class Time;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

// Interface for a class which provides profile related info in the ambient mode
// in ash.
class ASH_PUBLIC_EXPORT AmbientClient {
 public:
  using GetAccessTokenCallback =
      base::OnceCallback<void(const std::string& gaia_id,
                              const std::string& access_token,
                              const base::Time& expiration_time)>;

  static AmbientClient* Get();

  AmbientClient(const AmbientClient&) = delete;
  AmbientClient& operator=(const AmbientClient&) = delete;

  // Return whether the ambient mode is allowed for the user.
  virtual bool IsAmbientModeAllowed() = 0;

  virtual void SetAmbientModeAllowedForTesting(bool allowed) = 0;

  // Return the gaia and access token associated with the active user's profile.
  virtual void RequestAccessToken(GetAccessTokenCallback callback) = 0;

  // Downloads the image at given |url|.
  virtual void DownloadImage(
      const std::string& url,
      ash::ImageDownloader::DownloadCallback callback) = 0;

  // Return the URL loader factory associated with the active user's profile.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Return the URL loader factory associated with the sign in profile.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetSigninURLLoaderFactory() = 0;

  // Requests a connection to the device service's |WakeLockProvider|
  // from the browser.
  virtual void RequestWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) = 0;

  // Whether to use backend production server.
  virtual bool ShouldUseProdServer() = 0;

 protected:
  AmbientClient();
  virtual ~AmbientClient();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_CLIENT_H_
