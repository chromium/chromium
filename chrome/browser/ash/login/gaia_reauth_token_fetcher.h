// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_GAIA_REAUTH_TOKEN_FETCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_GAIA_REAUTH_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace ash {

// This class handles sending request to the Cryptohome recovery service (which
// in turn calls Gaia service) and receiving the encoded reauth request token
// for Cryptohome recovery flow in the response.
class GaiaReauthTokenFetcher {
 public:
  using FetchCompleteCallback = base::OnceCallback<void(const std::string&)>;

  explicit GaiaReauthTokenFetcher(FetchCompleteCallback callback);

  GaiaReauthTokenFetcher(const GaiaReauthTokenFetcher&) = delete;
  GaiaReauthTokenFetcher& operator=(const GaiaReauthTokenFetcher&) = delete;

  ~GaiaReauthTokenFetcher();

  // Sends a request to the Cryptohome recovery service to fetch a Gaia reauth
  // token.
  void Fetch();

 private:
  // Handles responses from the SimpleURLLoader.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // Called at the end of Fetch().
  FetchCompleteCallback callback_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Used for metrics:
  std::unique_ptr<base::ElapsedTimer> fetch_timer_;

  base::WeakPtrFactory<GaiaReauthTokenFetcher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_GAIA_REAUTH_TOKEN_FETCHER_H_
