// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_IDENTITY_MANAGER_IMPL_H_
#define CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_IDENTITY_MANAGER_IMPL_H_

#include <memory>

#include "chrome/browser/media/kaleidoscope/mojom/kaleidoscope.mojom.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {
class WebUI;
}  //  namespace content

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

class Profile;

class KaleidoscopeIdentityManagerImpl
    : public media::mojom::KaleidoscopeIdentityManager,
      public signin::IdentityManager::Observer {
 public:
  KaleidoscopeIdentityManagerImpl(
      mojo::PendingReceiver<media::mojom::KaleidoscopeIdentityManager> receiver,
      content::WebUI* web_ui);
  KaleidoscopeIdentityManagerImpl(
      mojo::PendingReceiver<media::mojom::KaleidoscopeIdentityManager> receiver,
      Profile* profile);

  KaleidoscopeIdentityManagerImpl(const KaleidoscopeIdentityManagerImpl&) =
      delete;
  KaleidoscopeIdentityManagerImpl& operator=(
      const KaleidoscopeIdentityManagerImpl&) = delete;
  ~KaleidoscopeIdentityManagerImpl() override;

  // media::mojom::KaleidoscopeIdentityManager implementation.
  void GetCredentials(GetCredentialsCallback cb) override;
  void SignIn() override;
  void AddObserver(
      mojo::PendingRemote<media::mojom::KaleidoscopeIdentityObserver> observer)
      override;

  // signin::IdentityManager::Observer implementation.
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;

 private:
  // Called when an access token request completes (successfully or not).
  void OnAccessTokenAvailable(GoogleServiceAuthError error,
                              signin::AccessTokenInfo access_token_info);

  // Helper for fetching OAuth2 access tokens. This is non-null iff an access
  // token request is currently in progress.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  // Pending credentials waiting on an access token.
  std::vector<GetCredentialsCallback> pending_callbacks_;

  // The current set of credentials.
  media::mojom::CredentialsPtr credentials_;

  signin::IdentityManager* identity_manager_;

  content::WebUI* web_ui_ = nullptr;
  Profile* profile_ = nullptr;

  mojo::RemoteSet<media::mojom::KaleidoscopeIdentityObserver>
      identity_observers_;

  mojo::Receiver<media::mojom::KaleidoscopeIdentityManager> receiver_;
};

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_IDENTITY_MANAGER_IMPL_H_
