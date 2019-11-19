// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_SIGNIN_DATA_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_SIGNIN_DATA_COUNTER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/browsing_data/core/counters/passwords_counter.h"
#include "device/fido/platform_credential_store.h"

namespace browsing_data {

class SigninDataCounter : public PasswordsCounter {
 public:
  class SigninDataResult : public PasswordsResult {
   public:
    SigninDataResult(const SigninDataCounter* source,
                     ResultInt num_passwords,
                     ResultInt num_webauthn_credentials,
                     bool sync_enabled,
                     std::vector<std::string> domain_examples);
    ~SigninDataResult() override;

    ResultInt WebAuthnCredentialsValue() const {
      return num_webauthn_credentials_;
    }

   private:
    ResultInt num_webauthn_credentials_;
  };

  explicit SigninDataCounter(
      scoped_refptr<password_manager::PasswordStore> password_store,
      syncer::SyncService* sync_service,
      std::unique_ptr<::device::fido::PlatformCredentialStore>
          opt_platform_credential_store);
  ~SigninDataCounter() override;

 private:
  int CountWebAuthnCredentials();
  std::unique_ptr<PasswordsResult> MakeResult() override;

  std::unique_ptr<::device::fido::PlatformCredentialStore> credential_store_;
};

}  // namespace browsing_data

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_SIGNIN_DATA_COUNTER_H_
