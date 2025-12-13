// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_win.h"

#include <objbase.h>

#include <windows.h>

#include <userenv.h>
#include <wrl/client.h>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/win/com_init_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/pref_names.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/install_util.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"

namespace os_crypt {

namespace {

AppBoundEncryptionOverridesForTesting* g_overrides_for_testing = nullptr;

ProtectionLevel AddFlags(ProtectionLevel protection_level,
                         elevation_service::EncryptFlags flags) {
  // Check protection_level fits into 8-bits.
  CHECK_EQ(protection_level, protection_level & 0xFF);

  uint32_t flag_value = 0;
  if (flags.use_latest_key) {
    flag_value |= elevation_service::internal::kFlagUseLatestKey;
  }
  // Double check flags fits into 24-bits. This is checked elsewhere too by
  // static_asserts.
  CHECK_EQ(flag_value, flag_value & 0xFFFFFF);

  return static_cast<ProtectionLevel>(
      elevation_service::internal::PackFlagsAndProtectionLevel(
          flag_value, protection_level));
}

}  // namespace

namespace features {
BASE_FEATURE(kAppBoundDataReencrypt, base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

SupportLevel GetAppBoundEncryptionSupportLevel(PrefService* local_state) {
  if (g_overrides_for_testing) {
    return g_overrides_for_testing->GetAppBoundEncryptionSupportLevel(
        local_state);
  }

  // Must be a system install.
  if (!install_static::IsSystemInstall()) {
    return SupportLevel::kNotSystemLevel;
  }

  const auto maybe_using_default_user_data_dir =
      chrome::IsUsingDefaultDataDirectory();

  if (!maybe_using_default_user_data_dir.has_value()) {
    return SupportLevel::kApiFailed;
  }

  // User data dir can be overridden by policy or by a command line option.
  if (!maybe_using_default_user_data_dir.value()) {
    return SupportLevel::kNotUsingDefaultUserDataDir;
  }

  // Policy allows disabling App-Bound encryption. Note, this will not disable
  // decryption of existing data.
  if (local_state->HasPrefPath(prefs::kApplicationBoundEncryptionEnabled) &&
      local_state->IsManagedPreference(
          prefs::kApplicationBoundEncryptionEnabled) &&
      !local_state->GetBoolean(prefs::kApplicationBoundEncryptionEnabled)) {
    return SupportLevel::kDisabledByPolicy;
  }

  // Note: this must be pulled from pref rather than calling
  // `IsLocalSyncEnabled` to ensure that it's managed.
  if (local_state->IsManagedPreference(
          syncer::prefs::kEnableLocalSyncBackend) &&
      local_state->GetBoolean(syncer::prefs::kEnableLocalSyncBackend)) {
    return SupportLevel::kDisabledByRoamingChromeProfile;
  }

  DWORD profile_type;
  if (::GetProfileType(&profile_type)) {
    // App-Bound binds the encryption key to the SYSTEM DPAPI key, which does
    // not roam with a roaming profile.
    if (profile_type > 0) {
      return SupportLevel::kDisabledByRoamingWindowsProfile;
    }
  }

  // https://learn.microsoft.com/en-us/fslogix/ is a roaming profile solution
  // that does not use profile type. Detect it explicitly here.
  for (const auto access_mask : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
    if (base::win::RegKey{}.Open(HKEY_LOCAL_MACHINE, L"SOFTWARE\\FSLogix",
                                 KEY_QUERY_VALUE | access_mask) ==
        ERROR_SUCCESS) {
      return SupportLevel::kDisabledByRoamingWindowsProfile;
    }
  }

  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return SupportLevel::kApiFailed;
  }

  // If the user data dir is on a network drive, then maybe it is shared between
  // multiple machines, which is unsupported since App-Bound more strongly binds
  // data to the local machine.
  if (user_data_dir.IsNetwork()) {
    return SupportLevel::kUserDataDirNotLocalDisk;
  }

  std::string image_path(MAX_PATH, L'\0');
  DWORD path_length = image_path.size();
  BOOL success =
      ::QueryFullProcessImageNameA(::GetCurrentProcess(), PROCESS_NAME_NATIVE,
                                   image_path.data(), &path_length);
  if (!success && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    // Process name is potentially greater than MAX_PATH, try larger max size.
    // https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
    image_path.resize(UNICODE_STRING_MAX_CHARS);
    path_length = image_path.size();
    success =
        ::QueryFullProcessImageNameA(::GetCurrentProcess(), PROCESS_NAME_NATIVE,
                                     image_path.data(), &path_length);
  }
  if (!success) {
    return SupportLevel::kApiFailed;
  }
  image_path.resize(path_length);

  // Must be running on a local disk.
  if (!base::StartsWith(image_path, "\\Device\\HarddiskVolume",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return SupportLevel::kNotLocalDisk;
  }

  return SupportLevel::kSupported;
}

HRESULT EncryptAppBoundString(ProtectionLevel protection_level,
                              const std::string& plaintext,
                              std::string& ciphertext,
                              DWORD& last_error,
                              elevation_service::EncryptFlags* flags) {
  if (g_overrides_for_testing) {
    return g_overrides_for_testing->EncryptAppBoundString(
        protection_level, plaintext, ciphertext, last_error, flags);
  }

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
  UNSAFE_TODO(::memcpy(plaintext_data.AllocateBytes(plaintext.length()),
                       plaintext.data(), plaintext.length()));

  base::win::ScopedBstr encrypted_data;
  if (flags) {
    protection_level = AddFlags(protection_level, *flags);
  }
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
                              ProtectionLevel protection_level,
                              std::optional<std::string>& new_ciphertext,
                              DWORD& last_error,
                              elevation_service::EncryptFlags* flags) {
  if (g_overrides_for_testing) {
    return g_overrides_for_testing->DecryptAppBoundString(
        ciphertext, plaintext, protection_level, new_ciphertext, last_error,
        flags);
  }

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
  UNSAFE_TODO(::memcpy(ciphertext_data.AllocateBytes(ciphertext.length()),
                       ciphertext.data(), ciphertext.length()));

  base::win::ScopedBstr plaintext_data;
  hr = elevator->DecryptData(ciphertext_data.Get(), plaintext_data.Receive(),
                             &last_error);
  if (FAILED(hr)) {
    return hr;
  }

  new_ciphertext = std::nullopt;

  if (base::FeatureList::IsEnabled(features::kAppBoundDataReencrypt) &&
      hr == elevation_service::Elevator::kSuccessShouldReencrypt) {
    DWORD encrypt_last_error;
    base::win::ScopedBstr reencrypted_data;
    if (flags) {
      protection_level = AddFlags(protection_level, *flags);
    }
    HRESULT encrypt_hr =
        elevator->EncryptData(protection_level, plaintext_data.Get(),
                              reencrypted_data.Receive(), &encrypt_last_error);
    if (SUCCEEDED(encrypt_hr)) {
      new_ciphertext.emplace(
          reinterpret_cast<std::string::value_type*>(reencrypted_data.Get()),
          reencrypted_data.ByteLength());
    }
  }

  plaintext.assign(
      reinterpret_cast<std::string::value_type*>(plaintext_data.Get()),
      plaintext_data.ByteLength());

  ::SecureZeroMemory(plaintext_data.Get(), plaintext_data.ByteLength());

  last_error = ERROR_SUCCESS;
  return S_OK;
}

void SetOverridesForTesting(AppBoundEncryptionOverridesForTesting* overrides) {
  CHECK(!g_overrides_for_testing || !overrides);
  g_overrides_for_testing = overrides;
}

}  // namespace os_crypt
