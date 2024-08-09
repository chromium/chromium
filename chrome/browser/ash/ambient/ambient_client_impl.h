// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_AMBIENT_AMBIENT_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_AMBIENT_AMBIENT_CLIENT_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/image_downloader.h"
#include "base/memory/weak_ptr.h"

class GoogleServiceAuthError;

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

// Class to provide profile related info.
class AmbientClientImpl : public ash::AmbientClient {
 public:
  AmbientClientImpl();
  ~AmbientClientImpl() override;

  // ash::AmbientClient:
  bool IsAmbientModeAllowed() override;
  void SetAmbientModeAllowedForTesting(bool allowed) override;
  void RequestAccessToken(GetAccessTokenCallback callback) override;
  void DownloadImage(const std::string& url,
                     ash::ImageDownloader::DownloadCallback callback) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSigninURLLoaderFactory()
      override;

  void RequestWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) override;
  bool ShouldUseProdServer() override;

  const std::map<base::UnguessableToken,
                 std::unique_ptr<signin::AccessTokenFetcher>>&
  token_fetchers_for_testing() const {
    return token_fetchers_;
  }

 private:
  void OnGetAccessToken(GetAccessTokenCallback callback,
                        base::UnguessableToken fetcher_id,
                        const std::string& gaia_id,
                        GoogleServiceAuthError error,
                        signin::AccessTokenInfo access_token_info);

  std::map<base::UnguessableToken, std::unique_ptr<signin::AccessTokenFetcher>>
      token_fetchers_;
  std::optional<bool> is_allowed_for_testing_;
  base::WeakPtrFactory<AmbientClientImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_AMBIENT_AMBIENT_CLIENT_IMPL_H_
