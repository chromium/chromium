// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_
#define CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class Profile;

namespace base {
class FilePath;
}

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

// ForceSigninVerifier will verify profile's auth token when profile is loaded
// into memory by the first time via gaia server. It will retry on any transient
// error.
class ForceSigninVerifier
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit ForceSigninVerifier(Profile* profile,
                               signin::IdentityManager* identity_manager);

  ForceSigninVerifier(const ForceSigninVerifier&) = delete;
  ForceSigninVerifier& operator=(const ForceSigninVerifier&) = delete;

  ~ForceSigninVerifier() override;

  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  // override network::NetworkConnectionTracker::NetworkConnectionObserver
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Cancel any pending or ongoing verification.
  void Cancel();

  // Return the value of |has_token_verified_|.
  bool HasTokenBeenVerified();

 protected:
  // Send the token verification request. The request will be sent only if
  //   - The token has never been verified before.
  //   - There is no on going verification.
  //   - There is network connection.
  //   - The profile has signed in.
  void SendRequest();

  // Send the request if |network_type| is not CONNECTION_NONE and
  // ShouldSendRequest returns true.
  void SendRequestIfNetworkAvailable(
      network::mojom::ConnectionType network_type);

  bool ShouldSendRequest();

  virtual void CloseAllBrowserWindows();
  void OnCloseBrowsersSuccess(const base::FilePath& profile_path);

  signin::PrimaryAccountAccessTokenFetcher* GetAccessTokenFetcherForTesting();
  net::BackoffEntry* GetBackoffEntryForTesting();
  base::OneShotTimer* GetOneShotTimerForTesting();

 private:
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Indicates whether the verification is finished successfully or with a
  // persistent error.
  bool has_token_verified_ = false;
  net::BackoffEntry backoff_entry_;
  base::OneShotTimer backoff_request_timer_;
  base::TimeTicks creation_time_;

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  base::WeakPtrFactory<ForceSigninVerifier> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_
