// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"

#include "base/win/registry.h"
#include "chrome/installer/util/install_util.h"
#include "crypto/unexportable_key.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {

SigningKeyPair::KeyInfo InvalidKeyInfo() {
  return {BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()};
}

class SigningKeyPairWin : public SigningKeyPair {
 public:
  // SigningKeyPair:
  std::unique_ptr<crypto::UnexportableKeyProvider> GetTpmBackedKeyProvider()
      override;
  bool StoreKeyPair(KeyTrustLevel trust_level,
                    std::vector<uint8_t> wrapped) override;
  KeyInfo LoadKeyPair() override;
};

std::unique_ptr<crypto::UnexportableKeyProvider>
SigningKeyPairWin::GetTpmBackedKeyProvider() {
  return crypto::GetUnexportableKeyProvider();
}

bool SigningKeyPairWin::StoreKeyPair(KeyTrustLevel trust_level,
                                     std::vector<uint8_t> wrapped) {
  base::win::RegKey key;
  std::wstring signingkey_name;
  std::wstring trustlevel_name;
  std::tie(key, signingkey_name, trustlevel_name) =
      InstallUtil::GetDeviceTrustSigningKeyLocation(
          InstallUtil::ReadOnly(false));
  if (!key.Valid())
    return false;

  return key.WriteValue(signingkey_name.c_str(), wrapped.data(), wrapped.size(),
                        REG_BINARY) == ERROR_SUCCESS &&
         key.WriteValue(trustlevel_name.c_str(), trust_level) == ERROR_SUCCESS;
}

SigningKeyPair::KeyInfo SigningKeyPairWin::LoadKeyPair() {
  base::win::RegKey key;
  std::wstring signingkey_name;
  std::wstring trustlevel_name;
  std::tie(key, signingkey_name, trustlevel_name) =
      InstallUtil::GetDeviceTrustSigningKeyLocation(
          InstallUtil::ReadOnly(true));
  if (!key.Valid())
    return InvalidKeyInfo();

  DWORD trust_level_dw;
  auto res = key.ReadValueDW(trustlevel_name.c_str(), &trust_level_dw);
  if (res != ERROR_SUCCESS)
    return InvalidKeyInfo();

  KeyTrustLevel trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  if (trust_level_dw == BPKUR::CHROME_BROWSER_TPM_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_TPM_KEY;
  } else if (trust_level_dw == BPKUR::CHROME_BROWSER_OS_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_OS_KEY;
  } else {
    return InvalidKeyInfo();
  }

  std::vector<uint8_t> wrapped;
  DWORD type = REG_NONE;
  DWORD size = 0;
  res = key.ReadValue(signingkey_name.c_str(), nullptr, &size, &type);
  if (res == ERROR_SUCCESS && type == REG_BINARY) {
    wrapped.resize(size);
    res = key.ReadValue(signingkey_name.c_str(), wrapped.data(), &size, &type);
  }
  if (res != ERROR_SUCCESS || type != REG_BINARY)
    return InvalidKeyInfo();

  return {trust_level, wrapped};
}

}  // namespace

// static
std::unique_ptr<SigningKeyPair> SigningKeyPair::CreatePlatformKeyPair() {
  return std::make_unique<SigningKeyPairWin>();
}

}  // namespace enterprise_connectors
