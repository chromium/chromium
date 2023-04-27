// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cryptauth/cryptauth_device_id_provider_impl.h"

#include "base/no_destructor.h"
#include "base/uuid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

// static
void CryptAuthDeviceIdProviderImpl::RegisterLocalPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kCryptAuthDeviceId, std::string());
}

// static
const CryptAuthDeviceIdProviderImpl*
CryptAuthDeviceIdProviderImpl::GetInstance() {
  static const base::NoDestructor<CryptAuthDeviceIdProviderImpl> provider;
  return provider.get();
}

std::string CryptAuthDeviceIdProviderImpl::GetDeviceId() const {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  std::string device_id = local_state->GetString(prefs::kCryptAuthDeviceId);
  if (device_id.empty()) {
    device_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
    local_state->SetString(prefs::kCryptAuthDeviceId, device_id);
  }

  return device_id;
}

CryptAuthDeviceIdProviderImpl::CryptAuthDeviceIdProviderImpl() = default;

}  // namespace ash
