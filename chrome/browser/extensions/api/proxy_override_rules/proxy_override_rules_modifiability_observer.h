// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROXY_OVERRIDE_RULES_PROXY_OVERRIDE_RULES_MODIFIABILITY_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROXY_OVERRIDE_RULES_PROXY_OVERRIDE_RULES_MODIFIABILITY_OBSERVER_H_

#include "base/scoped_observation.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/pref_modifiability_observer.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace extensions {

class ProxyOverrideRulesTransformer;

// Observes affiliation and policy changes that affect whether extensions can
// modify `kProxyOverrideRules` in a `Profile`.
class ProxyOverrideRulesModifiabilityObserver
    : public PrefModifiabilityObserver,
      public policy::PolicyService::ProviderUpdateObserver {
 public:
  ProxyOverrideRulesModifiabilityObserver(
      ProxyOverrideRulesTransformer* transformer,
      Profile* profile);

  ProxyOverrideRulesModifiabilityObserver(
      const ProxyOverrideRulesModifiabilityObserver&) = delete;
  ProxyOverrideRulesModifiabilityObserver& operator=(
      const ProxyOverrideRulesModifiabilityObserver&) = delete;

  ~ProxyOverrideRulesModifiabilityObserver() override;

  // policy::PolicyService::ProviderUpdateObserver:
  void OnProviderUpdatePropagated(
      policy::ConfigurationPolicyProvider* provider) override;

 private:
  PrefChangeRegistrar registrar_;
  base::ScopedObservation<policy::PolicyService,
                          policy::PolicyService::ProviderUpdateObserver>
      policy_service_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_OVERRIDE_RULES_PROXY_OVERRIDE_RULES_MODIFIABILITY_OBSERVER_H_
