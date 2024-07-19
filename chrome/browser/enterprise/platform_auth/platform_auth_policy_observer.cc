// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_policy_observer.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

PlatformAuthPolicyObserver::PlatformAuthPolicyObserver(
    PrefService* local_state) {
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      GetPrefName(),
      base::BindRepeating(&PlatformAuthPolicyObserver::OnPrefChanged,
                          base::Unretained(this)));
  // Initialize `PlatformAuthProviderManager` using the current policy value.
  OnPrefChanged();
}

// static
void PlatformAuthPolicyObserver::RegisterPrefs(
    PrefRegistrySimple* pref_registry) {
#if BUILDFLAG(IS_WIN)
  pref_registry->RegisterIntegerPref(GetPrefName(), 0);
#elif BUILDFLAG(IS_MAC)
  pref_registry->RegisterIntegerPref(GetPrefName(), 1);
#else
#error Unsupported platform
#endif
}

// static
const char* PlatformAuthPolicyObserver::GetPrefName() {
#if BUILDFLAG(IS_WIN)
  return prefs::kCloudApAuthEnabled;
#elif BUILDFLAG(IS_MAC)
  return prefs::kExtensibleEnterpriseSSOEnabled;
#else
#error Unsupported platform
#endif
}

void PlatformAuthPolicyObserver::OnPrefChanged() {
  // 0 == Disabled
  // 1 == Enabled
  const bool enabled =
      pref_change_registrar_.prefs()->GetInteger(GetPrefName()) != 0;
  enterprise_auth::PlatformAuthProviderManager::GetInstance().SetEnabled(
      enabled, base::OnceClosure());
}
