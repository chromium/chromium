// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_SIGNIN_DATA_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_SIGNIN_DATA_COUNTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "device/fido/platform_credential_store.h"

class PrefService;

namespace browsing_data {

class SigninDataCounter : public PasswordsCounter {
 public:
  class SigninDataResult : public PasswordsResult {
   public:
    SigninDataResult(const SigninDataCounter* source,
                     ResultInt num_passwords,
                     ResultInt num_account_passwords,
                     ResultInt num_webauthn_credentials,
                     bool sync_enabled,
                     std::vector<std::string> domain_examples,
                     std::vector<std::string> account_domain_examples);
    ~SigninDataResult() override;

    ResultInt WebAuthnCredentialsValue() const {
      return num_webauthn_credentials_;
    }

   private:
    ResultInt num_webauthn_credentials_;
  };

  SigninDataCounter(
      scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
      scoped_refptr<password_manager::PasswordStoreInterface> account_store,
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      std::unique_ptr<::device::fido::PlatformCredentialStore>
          opt_platform_credential_store);
  ~SigninDataCounter() override;

 private:
  void OnCountWebAuthnCredentialsFinished(size_t num_credentials);
  void CountWebAuthnCredentials(base::Time start, base::Time end);
  void Count() override;
  void OnPasswordsFetchDone() override;
  std::unique_ptr<PasswordsResult> MakeResult() override;

  std::unique_ptr<::device::fido::PlatformCredentialStore> credential_store_;
  bool passwords_counter_fetch_done_ = false;
  bool webauthn_credentials_fetch_done_ = false;
  int num_webauthn_credentials_ = 0;

  base::WeakPtrFactory<SigninDataCounter> weak_ptr_factory_{this};
};

}  // namespace browsing_data

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_SIGNIN_DATA_COUNTER_H_
