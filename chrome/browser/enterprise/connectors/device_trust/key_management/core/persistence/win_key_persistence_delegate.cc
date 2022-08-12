// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/win_key_persistence_delegate.h"

#include <string>
#include <utility>

#include "base/win/registry.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/installer/util/install_util.h"
#include "crypto/unexportable_key.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;

namespace enterprise_connectors {

WinKeyPersistenceDelegate::~WinKeyPersistenceDelegate() = default;

bool WinKeyPersistenceDelegate::CheckRotationPermissions() {
  return true;
}

bool WinKeyPersistenceDelegate::StoreKeyPair(
    KeyPersistenceDelegate::KeyTrustLevel trust_level,
    std::vector<uint8_t> wrapped) {
  base::win::RegKey key;
  std::wstring signingkey_name;
  std::wstring trustlevel_name;
  std::tie(key, signingkey_name, trustlevel_name) =
      InstallUtil::GetDeviceTrustSigningKeyLocation(
          InstallUtil::ReadOnly(false));
  if (!key.Valid())
    return false;

  if (trust_level == BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED) {
    DCHECK_EQ(wrapped.size(), 0u);
    return key.DeleteValue(signingkey_name.c_str()) == ERROR_SUCCESS &&
           key.DeleteValue(trustlevel_name.c_str()) == ERROR_SUCCESS;
  }

  return key.WriteValue(signingkey_name.c_str(), wrapped.data(), wrapped.size(),
                        REG_BINARY) == ERROR_SUCCESS &&
         key.WriteValue(trustlevel_name.c_str(), trust_level) == ERROR_SUCCESS;
}

std::unique_ptr<SigningKeyPair> WinKeyPersistenceDelegate::LoadKeyPair() {
  base::win::RegKey key;
  std::wstring signingkey_name;
  std::wstring trustlevel_name;
  std::tie(key, signingkey_name, trustlevel_name) =
      InstallUtil::GetDeviceTrustSigningKeyLocation(
          InstallUtil::ReadOnly(true));
  if (!key.Valid())
    return nullptr;

  DWORD trust_level_dw;
  auto res = key.ReadValueDW(trustlevel_name.c_str(), &trust_level_dw);
  if (res != ERROR_SUCCESS)
    return nullptr;

  std::unique_ptr<crypto::UnexportableKeyProvider> provider;
  KeyTrustLevel trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  if (trust_level_dw == BPKUR::CHROME_BROWSER_HW_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_HW_KEY;
    provider = crypto::GetUnexportableKeyProvider();
  } else if (trust_level_dw == BPKUR::CHROME_BROWSER_OS_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_OS_KEY;
    provider = std::make_unique<ECSigningKeyProvider>();
  } else {
    return nullptr;
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
    return nullptr;

  auto signing_key = provider->FromWrappedSigningKeySlowly(wrapped);
  if (!signing_key) {
    return nullptr;
  }

  return std::make_unique<SigningKeyPair>(std::move(signing_key), trust_level);
}

std::unique_ptr<SigningKeyPair> WinKeyPersistenceDelegate::CreateKeyPair() {
  auto acceptable_algorithms = {
      // This a temporary fix to bug b/240187326 where The unexportable key when
      // given the span of acceptable algorithms fails to create a TPM key using
      // the ECDSA_SHA256 algorithm but works for the RSA algorithm.
      crypto::SignatureVerifier::RSA_PKCS1_SHA256,
  };

  auto provider = crypto::GetUnexportableKeyProvider();
  KeyPersistenceDelegate::KeyTrustLevel trust_level =
      BPKUR::CHROME_BROWSER_HW_KEY;
  if (!provider) {
    acceptable_algorithms = {
        crypto::SignatureVerifier::ECDSA_SHA256,
        crypto::SignatureVerifier::RSA_PKCS1_SHA256,
    };
    provider = std::make_unique<ECSigningKeyProvider>();
    trust_level = BPKUR::CHROME_BROWSER_OS_KEY;
  }

  auto signing_key = provider->GenerateSigningKeySlowly(acceptable_algorithms);
  if (!signing_key) {
    return nullptr;
  }

  return std::make_unique<SigningKeyPair>(std::move(signing_key), trust_level);
}

}  // namespace enterprise_connectors
