// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service_test_util.h"

namespace chromeos {
namespace platform_keys {
namespace test_util {

Status StatusWaiter::status() {
  return Get();
}

const std::unique_ptr<std::vector<TokenId>>&
GetTokensExecutionWaiter::token_ids() {
  return Get<0>();
}

Status GetTokensExecutionWaiter::status() {
  return Get<1>();
}

const std::string& GenerateKeyExecutionWaiter::public_key_spki_der() {
  return Get<0>();
}

Status GenerateKeyExecutionWaiter::status() {
  return Get<1>();
}

base::OnceCallback<void(const std::string&, Status)>
GenerateKeyExecutionWaiter::GetCallback() {
  return TestFuture::GetCallback<const std::string&, Status>();
}

const std::string& SignExecutionWaiter::signature() {
  return Get<0>();
}

Status SignExecutionWaiter::status() {
  return Get<1>();
}

base::OnceCallback<void(const std::string&, Status)>
SignExecutionWaiter::GetCallback() {
  return TestFuture::GetCallback<const std::string&, Status>();
}

const net::CertificateList& GetCertificatesExecutionWaiter::matches() {
  return *Get<0>();
}

Status GetCertificatesExecutionWaiter::status() {
  return Get<1>();
}

const absl::optional<std::string>&
GetAttributeForKeyExecutionWaiter::attribute_value() {
  return Get<0>();
}

Status GetAttributeForKeyExecutionWaiter::status() {
  return Get<1>();
}

base::OnceCallback<void(const absl::optional<std::string>&, Status)>
GetAttributeForKeyExecutionWaiter::GetCallback() {
  return TestFuture::GetCallback<const absl::optional<std::string>&, Status>();
}

const std::vector<std::string>& GetAllKeysExecutionWaiter::public_keys() {
  return Get<0>();
}

Status GetAllKeysExecutionWaiter::status() {
  return Get<1>();
}

absl::optional<bool> IsKeyOnTokenExecutionWaiter::on_slot() {
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

}  // namespace test_util
}  // namespace platform_keys
}  // namespace chromeos
