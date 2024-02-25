// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/signin_data_counter.h"

#include <string>
#include <utility>

#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace browsing_data {

SigninDataCounter::SigninDataCounter(
    scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
    scoped_refptr<password_manager::PasswordStoreInterface> account_store,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    std::unique_ptr<::device::fido::PlatformCredentialStore>
        opt_platform_credential_store)
    : PasswordsCounter(profile_store,
                       account_store,
                       pref_service,
                       sync_service),
      credential_store_(std::move(opt_platform_credential_store)) {}

SigninDataCounter::~SigninDataCounter() = default;

void SigninDataCounter::OnCountWebAuthnCredentialsFinished(
    size_t num_credentials) {
  num_webauthn_credentials_ = num_credentials;
  webauthn_credentials_fetch_done_ = true;
  if (passwords_counter_fetch_done_)
    ReportResult(MakeResult());
}

void SigninDataCounter::CountWebAuthnCredentials(base::Time start,
                                                 base::Time end) {
  if (!credential_store_) {
    OnCountWebAuthnCredentialsFinished(0);
    return;
  }

  credential_store_->CountCredentials(
      start, end,
      base::BindOnce(&SigninDataCounter::OnCountWebAuthnCredentialsFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SigninDataCounter::Count() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  passwords_counter_fetch_done_ = webauthn_credentials_fetch_done_ = false;
  PasswordsCounter::Count();
  CountWebAuthnCredentials(GetPeriodStart(), GetPeriodEnd());
}

void SigninDataCounter::OnPasswordsFetchDone() {
  passwords_counter_fetch_done_ = true;
  if (webauthn_credentials_fetch_done_)
    ReportResult(MakeResult());
}

std::unique_ptr<PasswordsCounter::PasswordsResult>
SigninDataCounter::MakeResult() {
  return std::make_unique<SigninDataResult>(
      this, num_passwords(), num_account_passwords(), num_webauthn_credentials_,
      is_sync_active(), domain_examples(), account_domain_examples());
}

SigninDataCounter::SigninDataResult::SigninDataResult(
    const SigninDataCounter* source,
    ResultInt num_passwords,
    ResultInt num_account_passwords,
    ResultInt num_webauthn_credentials,
    bool sync_enabled,
    std::vector<std::string> domain_examples,
    std::vector<std::string> account_domain_examples)
    : PasswordsCounter::PasswordsResult(source,
                                        num_passwords,
                                        num_account_passwords,
                                        sync_enabled,
                                        std::move(domain_examples),
                                        std::move(account_domain_examples)),
      num_webauthn_credentials_(num_webauthn_credentials) {}

SigninDataCounter::SigninDataResult::~SigninDataResult() = default;

}  // namespace browsing_data
