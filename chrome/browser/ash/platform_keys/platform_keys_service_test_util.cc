// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/platform_keys_service_test_util.h"

#include "chrome/browser/ash/platform_keys/chaps_util.h"
#include "crypto/nss_key_util.h"

namespace ash {
namespace platform_keys {
namespace test_util {

using ::chromeos::platform_keys::Status;
using ::chromeos::platform_keys::TokenId;

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

FakeChapsUtil::FakeChapsUtil(OnKeyGenerated on_key_generated)
    : on_key_generated_(on_key_generated) {}
FakeChapsUtil::~FakeChapsUtil() = default;

bool FakeChapsUtil::GenerateSoftwareBackedRSAKey(
    PK11SlotInfo* slot,
    uint16_t num_bits,
    crypto::ScopedSECKEYPublicKey* out_public_key,
    crypto::ScopedSECKEYPrivateKey* out_private_key) {
  if (!crypto::GenerateRSAKeyPairNSS(slot, num_bits, /*permanent=*/true,
                                     out_public_key, out_private_key)) {
    return false;
  }
  crypto::ScopedSECItem spki_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(out_public_key->get()));
  on_key_generated_.Run(std::string(
      reinterpret_cast<const char*>(spki_der->data), spki_der->len));
  return true;
}

ScopedChapsUtilOverride::ScopedChapsUtilOverride() {
  ChapsUtil::SetFactoryForTesting(base::BindRepeating(
      &ScopedChapsUtilOverride::CreateChapsUtil, base::Unretained(this)));
}

ScopedChapsUtilOverride::~ScopedChapsUtilOverride() {
  ChapsUtil::SetFactoryForTesting(ChapsUtil::FactoryCallback());
}

std::unique_ptr<ChapsUtil> ScopedChapsUtilOverride::CreateChapsUtil() {
  return std::make_unique<FakeChapsUtil>(
      base::BindRepeating(&ScopedChapsUtilOverride::OnKeyGenerated,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ScopedChapsUtilOverride::OnKeyGenerated(const std::string& spki) {
  generated_key_spkis_.push_back(spki);
}

}  // namespace test_util
}  // namespace platform_keys
}  // namespace ash
