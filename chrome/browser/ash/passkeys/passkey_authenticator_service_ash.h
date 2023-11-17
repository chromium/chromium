// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PASSKEYS_PASSKEY_AUTHENTICATOR_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_PASSKEYS_PASSKEY_AUTHENTICATOR_SERVICE_ASH_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/passkeys.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace trusted_vault {
class TrustedVaultClient;
}

namespace webauthn {
class PasskeyModel;
}

namespace ash {

// Implements a crosapi interface for creating and asserting passkeys associated
// with the primary profile.
class PasskeyAuthenticatorServiceAsh
    : public crosapi::mojom::PasskeyAuthenticator,
      public KeyedService {
 public:
  // `account_info` must belong the primary profile. `passkey_model` and
  // `trusted_vault_client` must outlive this instance.
  PasskeyAuthenticatorServiceAsh(
      CoreAccountInfo account_info,
      webauthn::PasskeyModel* passkey_model,
      trusted_vault::TrustedVaultClient* trusted_vault_client);
  PasskeyAuthenticatorServiceAsh(const PasskeyAuthenticatorServiceAsh&) =
      delete;
  PasskeyAuthenticatorServiceAsh& operator=(PasskeyAuthenticatorServiceAsh&) =
      delete;
  ~PasskeyAuthenticatorServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<crosapi::mojom::PasskeyAuthenticator>
                        pending_receiver);

  void Assert(crosapi::mojom::AccountKeyPtr account_key,
              crosapi::mojom::PasskeyAssertionRequestPtr request,
              AssertCallback callback) override;

 private:
  struct RequestState {
    RequestState();
    ~RequestState();
    crosapi::mojom::PasskeyAssertionRequestPtr assert_request;
    AssertCallback pending_assert_callback;
    absl::optional<std::vector<uint8_t>> security_domain_secret;
  };

  void FetchTrustedVaultKeys(base::OnceCallback<void()> callback);
  void OnHaveTrustedVaultKeys(const std::vector<std::vector<uint8_t>>& keys);

  void DoAssert();
  void FinishAssert(crosapi::mojom::PasskeyAssertionResultPtr result);

  bool IsPrimaryAccount(const crosapi::mojom::AccountKeyPtr& account_key) const;

  const CoreAccountInfo primary_account_info_;
  const raw_ptr<webauthn::PasskeyModel> passkey_model_;
  const raw_ptr<trusted_vault::TrustedVaultClient> trusted_vault_client_;

  absl::optional<RequestState> request_state_;

  mojo::ReceiverSet<crosapi::mojom::PasskeyAuthenticator> receivers_;

  base::WeakPtrFactory<PasskeyAuthenticatorServiceAsh> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PASSKEYS_PASSKEY_AUTHENTICATOR_SERVICE_ASH_H_
