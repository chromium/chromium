// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/platform_keys_service_test_util.h"

#include "chrome/browser/chromeos/platform_keys/chaps_util.h"
#include "crypto/nss_key_util.h"

namespace ash::platform_keys::test_util {

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

const net::CertificateList& GetCertificatesExecutionWaiter::matches() {
  return *Get<0>();
}

Status GetCertificatesExecutionWaiter::status() {
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

// TODO(olsa): Extend this initial implementation with more logic.
bool FakeChapsUtil::ImportPkcs12Certificate(
    PK11SlotInfo* slot,
    const std::vector<uint8_t>& pkcs12_data,
    const std::string& password,
    bool is_software_backed) {
  return true;
}

ScopedChapsUtilOverride::ScopedChapsUtilOverride() {
  chromeos::platform_keys::ChapsUtil::SetFactoryForTesting(base::BindRepeating(
      &ScopedChapsUtilOverride::CreateChapsUtil, base::Unretained(this)));
}

ScopedChapsUtilOverride::~ScopedChapsUtilOverride() {
  chromeos::platform_keys::ChapsUtil::SetFactoryForTesting(
      chromeos::platform_keys::ChapsUtil::FactoryCallback());
}

std::unique_ptr<chromeos::platform_keys::ChapsUtil>
ScopedChapsUtilOverride::CreateChapsUtil() {
  return std::make_unique<FakeChapsUtil>(
      base::BindRepeating(&ScopedChapsUtilOverride::OnKeyGenerated,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ScopedChapsUtilOverride::OnKeyGenerated(const std::string& spki) {
  generated_key_spkis_.push_back(spki);
}

}  // namespace ash::platform_keys::test_util
