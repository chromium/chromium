// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_TACHYON_ICE_CONFIG_FETCHER_H_
#define CHROME_BROWSER_NEARBY_SHARING_TACHYON_ICE_CONFIG_FETCHER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc.mojom.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// Fetches a list of ICE servers using the Tachyon API. The lifetime of this
// class is expected to span multiple requests, and the most recently fetched
// ICE servers are cached in a private member variable.
class TachyonIceConfigFetcher : public ::sharing::mojom::IceConfigFetcher {
 public:
  TachyonIceConfigFetcher(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~TachyonIceConfigFetcher() override;

  TachyonIceConfigFetcher(const TachyonIceConfigFetcher& other) = delete;
  TachyonIceConfigFetcher& operator=(const TachyonIceConfigFetcher& other) =
      delete;

  // ::sharing::mojom::IceConfigFetcher
  void GetIceServers(GetIceServersCallback callback) override;

 private:
  void GetIceServersWithToken(GetIceServersCallback callback,
                              const std::string& token);

  void OnIceServersResponse(
      ::sharing::mojom::IceConfigFetcher::GetIceServersCallback callback,
      const std::string& request_id,
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      std::unique_ptr<std::string> response_body);

  // Parses the |GetIceServerResponse| proto provided by the Tachyon server and
  // caches a copy of the parsed result before returning it. Returns an empty
  // vector if parsing is unsuccessful. The |request_id| is for logging
  // purposes.
  std::vector<::sharing::mojom::IceServerPtr> ParseIceServersResponse(
      const std::string& serialized_proto,
      const std::string& request_id);

  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Cache the last retrieved ICE servers.
  std::optional<std::vector<::sharing::mojom::IceServerPtr>> ice_server_cache_;
  base::Time ice_server_cache_expiration_;

  base::WeakPtrFactory<TachyonIceConfigFetcher> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_TACHYON_ICE_CONFIG_FETCHER_H_
