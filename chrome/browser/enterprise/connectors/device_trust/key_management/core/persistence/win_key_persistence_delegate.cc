// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/win_key_persistence_delegate.h"

#include "base/win/registry.h"
#include "chrome/installer/util/install_util.h"

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

KeyPersistenceDelegate::KeyInfo WinKeyPersistenceDelegate::LoadKeyPair() {
  base::win::RegKey key;
  std::wstring signingkey_name;
  std::wstring trustlevel_name;
  std::tie(key, signingkey_name, trustlevel_name) =
      InstallUtil::GetDeviceTrustSigningKeyLocation(
          InstallUtil::ReadOnly(true));
  if (!key.Valid())
    return invalid_key_info();

  DWORD trust_level_dw;
  auto res = key.ReadValueDW(trustlevel_name.c_str(), &trust_level_dw);
  if (res != ERROR_SUCCESS)
    return invalid_key_info();

  KeyTrustLevel trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  if (trust_level_dw == BPKUR::CHROME_BROWSER_TPM_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_TPM_KEY;
  } else if (trust_level_dw == BPKUR::CHROME_BROWSER_OS_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_OS_KEY;
  } else {
    return invalid_key_info();
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
    return invalid_key_info();

  return {trust_level, wrapped};
}

std::unique_ptr<crypto::UnexportableKeyProvider>
WinKeyPersistenceDelegate::GetTpmBackedKeyProvider() {
  return crypto::GetUnexportableKeyProvider();
}

}  // namespace enterprise_connectors
