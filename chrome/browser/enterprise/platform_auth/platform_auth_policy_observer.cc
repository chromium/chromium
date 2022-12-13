// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_policy_observer.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

PlatformAuthPolicyObserver::PlatformAuthPolicyObserver(
    PrefService* local_state) {
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      prefs::kCloudApAuthEnabled,
      base::BindRepeating(&PlatformAuthPolicyObserver::OnPrefChanged,
                          base::Unretained(this)));
  // Initialize `PlatformAuthProviderManager` using the current policy value.
  OnPrefChanged();
}

// static
void PlatformAuthPolicyObserver::RegisterPrefs(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterIntegerPref(prefs::kCloudApAuthEnabled, 0);
}

void PlatformAuthPolicyObserver::OnPrefChanged() {
  // 0 == Disabled
  // 1 == Enabled
  const bool enabled =
      base::FeatureList::IsEnabled(enterprise_auth::kCloudApAuth) &&
      pref_change_registrar_.prefs()->GetInteger(prefs::kCloudApAuthEnabled) !=
          0;
  enterprise_auth::PlatformAuthProviderManager::GetInstance().SetEnabled(
      enabled, base::OnceClosure());
}
