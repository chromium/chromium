// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/signin_data_counter.h"

namespace browsing_data {

SigninDataCounter::SigninDataCounter(
    scoped_refptr<password_manager::PasswordStore> store,
    syncer::SyncService* sync_service,
    std::unique_ptr<::device::fido::PlatformCredentialStore>
        opt_platform_credential_store)
    : PasswordsCounter(store, sync_service),
      credential_store_(std::move(opt_platform_credential_store)) {}

SigninDataCounter::~SigninDataCounter() = default;

int SigninDataCounter::CountWebAuthnCredentials() {
  return credential_store_ ? credential_store_->CountCredentials(
                                 GetPeriodStart(), GetPeriodEnd())
                           : 0;
}

std::unique_ptr<BrowsingDataCounter::SyncResult>
SigninDataCounter::MakeResult() {
  return std::make_unique<SigninDataResult>(
      this, num_passwords(), CountWebAuthnCredentials(), is_sync_active());
}

SigninDataCounter::SigninDataResult::SigninDataResult(
    const SigninDataCounter* source,
    ResultInt num_passwords,
    ResultInt num_webauthn_credentials,
    bool sync_enabled)
    : BrowsingDataCounter::SyncResult(source, num_passwords, sync_enabled),
      num_webauthn_credentials_(num_webauthn_credentials) {}

SigninDataCounter::SigninDataResult::~SigninDataResult() {}

}  // namespace browsing_data
