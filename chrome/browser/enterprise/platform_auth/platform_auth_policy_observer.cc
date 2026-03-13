// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_policy_observer.h"

#include <stdint.h>

#include <memory>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_MAC)
#include "base/feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "components/enterprise/platform_auth/platform_auth_features.h"
#include "components/policy/core/common/management/management_service.h"
#endif  //  BUILFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#endif

PlatformAuthPolicyObserver::PlatformAuthPolicyObserver(
    PrefService* local_state) {
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      GetPrefName(),
      base::BindRepeating(&PlatformAuthPolicyObserver::OnPrefChanged,
                          base::Unretained(this)));

  // Initialize `PlatformAuthProviderManager` using the current policy
  // value.
  OnPrefChanged();
}

PlatformAuthPolicyObserver::~PlatformAuthPolicyObserver() = default;

// static
void PlatformAuthPolicyObserver::RegisterPrefs(
    PrefRegistrySimple* pref_registry) {
#if BUILDFLAG(IS_WIN)
  pref_registry->RegisterIntegerPref(GetPrefName(), 0);
#elif BUILDFLAG(IS_MAC)
  pref_registry->RegisterIntegerPref(GetPrefName(), 1);
  pref_registry->RegisterListPref(
      prefs::kExtensibleEnterpriseSSOEnabledIdps,
      enterprise_auth::ExtensibleEnterpriseSSOProvider::
          GetSupportedIdentityProvidersList());
  enterprise_auth::ExtensibleEnterpriseSSOPrefsHandler::RegisterPrefs(
      pref_registry);
#elif BUILDFLAG(IS_ANDROID)
  // TODO: b/484014627 - change the default value to 0 once policy is in place.
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
#elif BUILDFLAG(IS_ANDROID)
  return prefs::kAndroidEntraSSOEnabled;
#else
#error Unsupported platform
#endif
}

void PlatformAuthPolicyObserver::OnPrefChanged() {
  // 0 == Disabled
  // 1 == Enabled
  const bool enabled =
#if BUILDFLAG(IS_MAC)
      base::FeatureList::IsEnabled(
          enterprise_auth::kEnableExtensibleEnterpriseSSO) &&
      policy::ManagementServiceFactory::GetForPlatform()->IsManaged() &&
#elif BUILDFLAG(IS_ANDROID)
      base::FeatureList::IsEnabled(enterprise_auth::kAndroidEntraSSO) &&
#endif
      pref_change_registrar_.prefs()->GetInteger(GetPrefName()) != 0;

#if BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(enterprise_auth::kOktaSSO) && enabled &&
      !prefs_handler_) {
    prefs_handler_ =
        std::make_unique<enterprise_auth::ExtensibleEnterpriseSSOPrefsHandler>(
            pref_change_registrar_.prefs());
    prefs_handler_->UpdatePrefs();
  } else if (base::FeatureList::IsEnabled(enterprise_auth::kOktaSSO) &&
             !enabled && prefs_handler_) {
    prefs_handler_.reset();
  }
#endif

  VLOG(1) << "PlatformAuthProviderManager enabled: " << enabled;
  enterprise_auth::PlatformAuthProviderManager::GetInstance().SetEnabled(
      enabled, base::OnceClosure());
}
