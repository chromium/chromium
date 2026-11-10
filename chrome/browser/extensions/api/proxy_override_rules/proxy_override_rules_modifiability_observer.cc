// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/proxy_override_rules/proxy_override_rules_modifiability_observer.h"

#include "base/functional/bind.h"
#include "chrome/browser/extensions/api/proxy_override_rules/proxy_override_rules_transformer.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

ProxyOverrideRulesModifiabilityObserver::
    ProxyOverrideRulesModifiabilityObserver(
        ProxyOverrideRulesTransformer* transformer,
        Profile* profile)
    : PrefModifiabilityObserver(transformer, profile) {
  registrar_.Init(profile->GetPrefs());
  registrar_.Add(proxy_config::prefs::kEnableProxyOverrideRulesForAllUsers,
                 base::BindRepeating(&ProxyOverrideRulesModifiabilityObserver::
                                         MaybeNotifyModifiabilityChanged,
                                     base::Unretained(this)));
  if (profile->GetProfilePolicyConnector() &&
      profile->GetProfilePolicyConnector()->policy_service()) {
    policy_service_observation_.Observe(
        profile->GetProfilePolicyConnector()->policy_service());
  }
}

ProxyOverrideRulesModifiabilityObserver::
    ~ProxyOverrideRulesModifiabilityObserver() = default;

void ProxyOverrideRulesModifiabilityObserver::OnProviderUpdatePropagated(
    policy::ConfigurationPolicyProvider* provider) {
  MaybeNotifyModifiabilityChanged();
}

}  // namespace extensions
