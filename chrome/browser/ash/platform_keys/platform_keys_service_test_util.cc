// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/platform_keys_service_test_util.h"

#include "crypto/nss_key_util.h"

namespace ash::platform_keys::test_util {

using ::chromeos::platform_keys::Status;
using ::chromeos::platform_keys::TokenId;

Status StatusWaiter::status() {
  return Get();
}

const std::vector<TokenId>& GetTokensExecutionWaiter::token_ids() {
  return Get<0>();
}

Status GetTokensExecutionWaiter::status() {
  return Get<1>();
}

const net::CertificateList& GetCertificatesExecutionWaiter::matches() {
  return *Get<0>();
}

Status GetCertificatesExecutionWaiter::status() {
  return Get<1>();
}

std::optional<bool> IsKeyOnTokenExecutionWaiter::on_slot() {
  return Get<0>();
}

Status IsKeyOnTokenExecutionWaiter::status() {
  return Get<1>();
}

const std::vector<TokenId>& GetKeyLocationsExecutionWaiter::key_locations() {
  return Get<0>();
}

Status GetKeyLocationsExecutionWaiter::status() {
  return Get<1>();
}

base::OnceCallback<void(const std::vector<TokenId>&, Status)>
GetKeyLocationsExecutionWaiter::GetCallback() {
  return TestFuture::GetCallback<const std::vector<TokenId>&, Status>();
}

}  // namespace ash::platform_keys::test_util
