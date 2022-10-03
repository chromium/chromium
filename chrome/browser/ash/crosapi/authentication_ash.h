// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_AUTHENTICATION_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_AUTHENTICATION_ASH_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/crosapi/mojom/authentication.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {
class ExtendedAuthenticator;
class AuthenticationError;
}  // namespace ash

namespace extensions {
class QuickUnlockPrivateGetAuthTokenHelper;

namespace api {
namespace quick_unlock_private {
struct TokenInfo;
}  // namespace quick_unlock_private
}  // namespace api
}  // namespace extensions

namespace crosapi {

// This class is the ash-chrome implementation of the Authentication
// interface. This class must only be used from the main thread.
class AuthenticationAsh : public mojom::Authentication {
 public:
  using TokenInfo = extensions::api::quick_unlock_private::TokenInfo;

  AuthenticationAsh();
  AuthenticationAsh(const AuthenticationAsh&) = delete;
  AuthenticationAsh& operator=(const AuthenticationAsh&) = delete;
  ~AuthenticationAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Authentication> receiver);

  // crosapi::mojom::Authentication:
  void CreateQuickUnlockPrivateTokenInfo(
      const std::string& password,
      CreateQuickUnlockPrivateTokenInfoCallback callback) override;
  void IsOsReauthAllowedForActiveUserProfile(
      base::TimeDelta auth_token_lifetime,
      IsOsReauthAllowedForActiveUserProfileCallback callback) override;

 private:
  // Continuation of CreateQuickUnlockPrivateTokenInfo(). Last 3 params match
  // extensions::LegacyQuickUnlockPrivateGetAuthTokenHelper::ResultCallback.
  void OnLegacyCreateQuickUnlockPrivateTokenInfoResults(
      CreateQuickUnlockPrivateTokenInfoCallback callback,
      scoped_refptr<ash::ExtendedAuthenticator> extended_authenticator,
      bool success,
      std::unique_ptr<TokenInfo> token_info,
      const std::string& error_message);

  // Continuation of CreateQuickUnlockPrivateTokenInfo(). The last 2 params
  // match extensions::QuickUnlockPrivateGetAuthTokenHelper::ResultCallback.
  // The first argument is ignored; it is only there so that we can keep the
  // token helper alive.
  void OnCreateQuickUnlockPrivateTokenInfoResults(
      std::unique_ptr<extensions::QuickUnlockPrivateGetAuthTokenHelper>,
      CreateQuickUnlockPrivateTokenInfoCallback callback,
      absl::optional<TokenInfo>,
      absl::optional<ash::AuthenticationError>);

  mojo::ReceiverSet<mojom::Authentication> receivers_;

  base::WeakPtrFactory<AuthenticationAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_AUTHENTICATION_ASH_H_
