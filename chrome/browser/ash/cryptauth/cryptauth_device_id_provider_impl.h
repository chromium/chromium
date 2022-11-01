// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CRYPTAUTH_CRYPTAUTH_DEVICE_ID_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_CRYPTAUTH_CRYPTAUTH_DEVICE_ID_PROVIDER_IMPL_H_

#include <string>

#include "base/no_destructor.h"
#include "chromeos/ash/services/device_sync/public/cpp/cryptauth_device_id_provider.h"

class PrefRegistrySimple;

namespace ash {

// Concrete CryptAuthDeviceIdProvider implementation which stores the device ID
// in the browser process' local state PrefStore.
class CryptAuthDeviceIdProviderImpl
    : public device_sync::CryptAuthDeviceIdProvider {
 public:
  // Registers the prefs used by this class. |registry| must be associated
  // with browser local storage, not an individual profile.
  static void RegisterLocalPrefs(PrefRegistrySimple* registry);

  static const CryptAuthDeviceIdProviderImpl* GetInstance();

  CryptAuthDeviceIdProviderImpl(const CryptAuthDeviceIdProviderImpl&) = delete;
  CryptAuthDeviceIdProviderImpl& operator=(
      const CryptAuthDeviceIdProviderImpl&) = delete;

  // CryptAuthDeviceIdProvider:
  std::string GetDeviceId() const override;

 private:
  friend class base::NoDestructor<CryptAuthDeviceIdProviderImpl>;

  CryptAuthDeviceIdProviderImpl();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CRYPTAUTH_CRYPTAUTH_DEVICE_ID_PROVIDER_IMPL_H_
