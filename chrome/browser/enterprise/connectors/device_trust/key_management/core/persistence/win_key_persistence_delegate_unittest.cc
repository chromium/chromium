// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/win_key_persistence_delegate.h"

#include <string>
#include <utility>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "chrome/install_static/install_details.h"
#include "chrome/installer/util/install_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/unexportable_key.h"
#include "testing/gtest/include/gtest/gtest.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace {

// Returns the device trust signing key path.
std::wstring GetKeyPath() {
  std::wstring key_path = L"SOFTWARE\\";
  install_static::AppendChromeInstallSubDirectory(
      install_static::InstallDetails::Get().mode(), /*include_suffix=*/false,
      &key_path)
      .append(L"\\DeviceTrust");
  return key_path;
}

void ValidateSigningKey(enterprise_connectors::SigningKeyPair* key_pair,
                        BPKUR::KeyTrustLevel trust_level) {
  ASSERT_TRUE(key_pair);

  EXPECT_EQ(trust_level, key_pair->trust_level());

  if (trust_level == BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED) {
    ASSERT_TRUE(!key_pair->key());
    return;
  }

  ASSERT_TRUE(key_pair->key());

  // Extract a pubkey should work.
  std::vector<uint8_t> pubkey = key_pair->key()->GetSubjectPublicKeyInfo();
  EXPECT_TRUE(pubkey.size() > 0u);

  // Signing should work.
  auto signed_data = key_pair->key()->SignSlowly(
      base::as_bytes(base::make_span("data to sign")));
  ASSERT_TRUE(signed_data.has_value());
  EXPECT_TRUE(signed_data->size() > 0u);
}

}  // namespace

namespace enterprise_connectors {

class WinKeyPersistenceDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    persistence_delegate_ = absl::WrapUnique(new WinKeyPersistenceDelegate());
    key_path_ = GetKeyPath();
    registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE, &key_path_);
  }

 protected:
  // Sets a valid device trust key pair registry key in the device trust signing
  // key path.
  void SetRegistryKeyInfo(
      KeyPersistenceDelegate::KeyTrustLevel trust_level =
          BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED,
      std::vector<uint8_t> wrapped = std::vector<uint8_t>()) {
    base::win::RegKey key;
    std::wstring signingkey_name;
    std::wstring trustlevel_name;

    std::tie(key, signingkey_name, trustlevel_name) =
        InstallUtil::GetDeviceTrustSigningKeyLocation(
            InstallUtil::ReadOnly(false));

    EXPECT_TRUE(key.WriteValue(signingkey_name.c_str(), wrapped.data(),
                               wrapped.size(), REG_BINARY) == ERROR_SUCCESS);
    EXPECT_TRUE(key.WriteValue(trustlevel_name.c_str(), trust_level) ==
                ERROR_SUCCESS);
  }

  // Sets an ECDSA_SHA256 key algorithm for generating an unexportable key.
  // This is used because it is the only algorithm the scoped unexportable key
  // provider supports.
  void SetAcceptableTestingAlgorithm() {
    auto acceptable_algorithms = {
        crypto::SignatureVerifier::ECDSA_SHA256,
    };
    persistence_delegate_->SetAcceptableKeyAlgorithmForTesting(
        acceptable_algorithms);
  }

  std::unique_ptr<WinKeyPersistenceDelegate> persistence_delegate_;
  std::wstring key_path_;
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

// Tests the delete key edge case when a key already exists, and storing a new
// key with an unspecified trust level results in the existing key being
// deleted.
TEST_F(WinKeyPersistenceDelegateTest, DeleteKey) {
  SetRegistryKeyInfo();
  EXPECT_TRUE(persistence_delegate_->StoreKeyPair(
      BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()));
  EXPECT_FALSE(persistence_delegate_->LoadKeyPair());
}

// Tests creating an OS key pair when the EC key algorithm is not set and the
// TPM key provider fails to create a TPM key. By design we fall back on to the
// OS key provider and create an OS key pair. Also tests storing and loading the
// key pair.
TEST_F(WinKeyPersistenceDelegateTest, ValidOsKeyPair_Success) {
  auto key_pair = persistence_delegate_->CreateKeyPair();
  auto trust_level = BPKUR::CHROME_BROWSER_OS_KEY;
  ValidateSigningKey(key_pair.get(), trust_level);

  SetRegistryKeyInfo();
  EXPECT_TRUE(persistence_delegate_->StoreKeyPair(
      trust_level, key_pair->key()->GetWrappedKey()));

  SetRegistryKeyInfo(trust_level, key_pair->key()->GetWrappedKey());
  auto loaded_key_pair = persistence_delegate_->LoadKeyPair();
  EXPECT_EQ(key_pair.get()->key()->GetWrappedKey(),
            loaded_key_pair.get()->key()->GetWrappedKey());
}

// Tests creating a hardware key pair when the EC key algorithm is set and the
// TPM key provider successfully creates the hardware generated key. Also tests
// storing and loading the key pair.
TEST_F(WinKeyPersistenceDelegateTest, ValidHardwareKeyPair_Success) {
  SetAcceptableTestingAlgorithm();
  auto key_pair = persistence_delegate_->CreateKeyPair();
  auto trust_level = BPKUR::CHROME_BROWSER_HW_KEY;
  ValidateSigningKey(key_pair.get(), trust_level);

  SetRegistryKeyInfo();
  EXPECT_TRUE(persistence_delegate_->StoreKeyPair(
      trust_level, key_pair->key()->GetWrappedKey()));

  SetRegistryKeyInfo(trust_level, key_pair->key()->GetWrappedKey());
  auto loaded_key_pair = persistence_delegate_->LoadKeyPair();
  EXPECT_EQ(key_pair.get()->key()->GetWrappedKey(),
            loaded_key_pair.get()->key()->GetWrappedKey());
}

}  // namespace enterprise_connectors
