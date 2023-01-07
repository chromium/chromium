// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_win.h"

#include <objbase.h>
#include <string.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/logging.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/install_static/install_util.h"

namespace os_crypt {

HRESULT EncryptAppBoundString(ProtectionLevel protection_level,
                              const std::string& plaintext,
                              std::string& ciphertext,
                              DWORD& last_error) {
  base::win::AssertComInitialized();
  Microsoft::WRL::ComPtr<IElevator> elevator;
  last_error = ERROR_GEN_FAILURE;
  HRESULT hr = ::CoCreateInstance(
      install_static::GetElevatorClsid(), nullptr, CLSCTX_LOCAL_SERVER,
      install_static::GetElevatorIid(), IID_PPV_ARGS_Helper(&elevator));

  if (FAILED(hr))
    return hr;

  hr = ::CoSetProxyBlanket(
      elevator.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
  if (FAILED(hr))
    return hr;

  base::win::ScopedBstr plaintext_data;
  ::memcpy(plaintext_data.AllocateBytes(plaintext.length()), plaintext.data(),
           plaintext.length());

  base::win::ScopedBstr encrypted_data;
  hr = elevator->EncryptData(protection_level, plaintext_data.Get(),
                             encrypted_data.Receive(), &last_error);
  if (FAILED(hr))
    return hr;

  ciphertext.assign(
      reinterpret_cast<std::string::value_type*>(encrypted_data.Get()),
      encrypted_data.ByteLength());

  last_error = ERROR_SUCCESS;
  return S_OK;
}

HRESULT DecryptAppBoundString(const std::string& ciphertext,
                              std::string& plaintext,
                              DWORD& last_error) {
  DCHECK(!ciphertext.empty());
  base::win::AssertComInitialized();
  Microsoft::WRL::ComPtr<IElevator> elevator;
  last_error = ERROR_GEN_FAILURE;
  HRESULT hr = ::CoCreateInstance(
      install_static::GetElevatorClsid(), nullptr, CLSCTX_LOCAL_SERVER,
      install_static::GetElevatorIid(), IID_PPV_ARGS_Helper(&elevator));

  if (FAILED(hr))
    return hr;

  hr = ::CoSetProxyBlanket(
      elevator.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
  if (FAILED(hr))
    return hr;

  base::win::ScopedBstr ciphertext_data;
  ::memcpy(ciphertext_data.AllocateBytes(ciphertext.length()),
           ciphertext.data(), ciphertext.length());

  base::win::ScopedBstr plaintext_data;
  hr = elevator->DecryptData(ciphertext_data.Get(), plaintext_data.Receive(),
                             &last_error);
  if (FAILED(hr))
    return hr;

  plaintext.assign(
      reinterpret_cast<std::string::value_type*>(plaintext_data.Get()),
      plaintext_data.ByteLength());

  last_error = ERROR_SUCCESS;
  return S_OK;
}

}  // namespace os_crypt
