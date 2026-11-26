// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/app_bound_encryption_state_win.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_service.h"

namespace device_signals {

namespace {

AppBoundEncryptionState MapSupportLevel(os_crypt::SupportLevel support_level) {
  switch (support_level) {
    case os_crypt::SupportLevel::kDisabledByPolicy:
      return AppBoundEncryptionState::kDisabledByPolicy;
    case os_crypt::SupportLevel::kDisabledByRoamingWindowsProfile:
    case os_crypt::SupportLevel::kDisabledByRoamingChromeProfile:
      return AppBoundEncryptionState::kUnsupportedRoamingProfile;
    case os_crypt::SupportLevel::kNotSystemLevel:
    case os_crypt::SupportLevel::kNotLocalDisk:
    case os_crypt::SupportLevel::kNotUsingDefaultUserDataDir:
    case os_crypt::SupportLevel::kUserDataDirNotLocalDisk:
      return AppBoundEncryptionState::kUnsupportedInstallOrPath;
    case os_crypt::SupportLevel::kApiFailed:
      return AppBoundEncryptionState::kServiceFailure;
    case os_crypt::SupportLevel::kSupported:
      // `MapSupportLevel()` is only called when `"v20"` is not the active
      // encryption provider in `Encryptor`. If all environment and policy
      // checks currently pass (`kSupported`), `"v20"` failed key initialization
      // with the elevation service (or `ApplicationBoundEncryptionEnabled`,
      // which does not support dynamic refresh, was enabled at runtime and is
      // awaiting a browser restart).
      return AppBoundEncryptionState::kServiceFailure;
  }
}

void OnEncryptorReceived(
    PrefService* local_state,
    base::OnceCallback<void(AppBoundEncryptionState)> callback,
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  CHECK(encryptor);
  if (encryptor->provider_for_encryption() ==
      os_crypt_async::kAppBoundDataPrefix) {
    std::move(callback).Run(AppBoundEncryptionState::kEnabled);
    return;
  }

  std::move(callback).Run(MapSupportLevel(
      os_crypt::GetAppBoundEncryptionSupportLevel(local_state)));
}

}  // namespace

void GetAppBoundEncryptionState(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    PrefService* local_state,
    base::OnceCallback<void(AppBoundEncryptionState)> callback) {
  CHECK(os_crypt_async);
  CHECK(local_state);
  os_crypt_async->GetInstance(
      base::BindOnce(&OnEncryptorReceived, local_state, std::move(callback)));
}

}  // namespace device_signals
