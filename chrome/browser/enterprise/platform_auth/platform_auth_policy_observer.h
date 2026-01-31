// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_POLICY_OBSERVER_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_POLICY_OBSERVER_H_

#include "components/prefs/pref_change_registrar.h"
#if BUILDFLAG(IS_MAC)
#include <memory>
#endif

class PrefRegistrySimple;
class PrefService;

#if BUILDFLAG(IS_MAC)
namespace enterprise_auth {
class ExtensibleEnterpriseSSOPrefsHandler;
}
#endif

// Monitors the preference (backed by administrative policy settings) that
// controls SSO and applies its value to the platform authentication manager.
class PlatformAuthPolicyObserver {
 public:
  explicit PlatformAuthPolicyObserver(PrefService* local_state);
  ~PlatformAuthPolicyObserver();
  PlatformAuthPolicyObserver(const PlatformAuthPolicyObserver&) = delete;
  PlatformAuthPolicyObserver& operator=(const PlatformAuthPolicyObserver&) =
      delete;

  static const char* GetPrefName();
  static void RegisterPrefs(PrefRegistrySimple* pref_registry);

 private:
  void OnPrefChanged();

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<enterprise_auth::ExtensibleEnterpriseSSOPrefsHandler>
      prefs_handler_;
#endif
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_POLICY_OBSERVER_H_
