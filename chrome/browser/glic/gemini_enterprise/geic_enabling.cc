// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/glic/gemini_enterprise/geic_guest_url.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/google_groups_manager.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace geic {

namespace {

const policy::PolicyMap::Entry* GetGeminiEnterpriseUrlPolicyEntry(
    Profile* profile) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (!profile || !profile->GetProfilePolicyConnector() ||
      !profile->GetProfilePolicyConnector()->policy_service()) {
    return nullptr;
  }
  const policy::PolicyMap& policies =
      profile->GetProfilePolicyConnector()->policy_service()->GetPolicies(
          policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  return policies.Get(policy::key::kGeminiEnterpriseUrl);
#else
  return nullptr;
#endif
}

bool IsConsumerGeminiAllowedByPolicy(Profile* profile) {
  if (!profile || !profile->GetPrefs()) {
    return false;
  }
  const PrefService::Preference* pref = profile->GetPrefs()->FindPreference(
      optimization_guide::prefs::kGeminiSettings);
  return !pref ||
         pref->GetValue()->GetIfInt() ==
             std::to_underlying(optimization_guide::prefs::
                                    GeminiSettingsPolicyState::kEnabled);
}

bool IsPersonalProfile(Profile* profile) {
  return profile && !profile->IsOffTheRecord() &&
         profile->GetProfilePolicyConnector() &&
         !profile->GetProfilePolicyConnector()->IsManaged();
}

// Command-line override (`--geic-guest-url`), which can also be set from
// `chrome://flags`. Accepts the same URLs as Finch, or any HTTP or HTTPS URL
// with `--geic-allow-any-guest-url` for local development and tests.
base::expected<GeicGuestUrl, GuestUrlError> GetSwitchGuestUrl() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return GeicGuestUrl::FromSwitch(
      command_line->GetSwitchValueASCII(kGeicGuestURLSwitch),
      command_line->HasSwitch(kGeicAllowAnyGuestURLSwitch));
}

base::expected<GeicGuestUrl, GuestUrlError> GetPolicyGuestUrl(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile && profile->GetPrefs()
             ? GeicGuestUrl::FromPolicy(profile->GetPrefs()->GetString(
                   glic::prefs::kGlicGeminiEnterpriseUrl))
             : base::unexpected(GuestUrlError::kUnset);
}

base::expected<GeicGuestUrl, GuestUrlError> GetFinchGuestUrl(
    content::BrowserContext* browser_context) {
  if (browser_context) {
    const GoogleGroupsManager* google_groups_manager =
        GoogleGroupsManagerFactory::GetForBrowserContext(browser_context);
    if (google_groups_manager &&
        !google_groups_manager->IsFeatureEnabledForProfile(features::kGeic)) {
      return base::unexpected(GuestUrlError::kUnset);
    }
  }
  return GeicGuestUrl::FromFinch(features::kGeicGuestURL.Get());
}

}  // namespace

std::optional<GeicGuestUrl> ResolveGeicGuestUrl(
    content::BrowserContext* browser_context) {
  if (!IsGeicEnabledByFeature()) {
    return std::nullopt;
  }

  // `--geic-guest-url` is evaluated before the enterprise policy to simplify
  // development and local testing. Policy only accepts production Gemini
  // Enterprise hosts, Finch also accepts pre-production and internal corp
  // hosts, and the switch also accepts any HTTP or HTTPS URL with
  // `--geic-allow-any-guest-url`.
  base::expected<GeicGuestUrl, GuestUrlError> candidates[] = {
      GetSwitchGuestUrl(),
      GetPolicyGuestUrl(browser_context),
      GetFinchGuestUrl(browser_context),
  };
  // The first set candidate wins, even if it is invalid, so that an invalid
  // higher-priority source doesn't silently fall through.
  auto it = std::ranges::find_if(
      candidates,
      [](const base::expected<GeicGuestUrl, GuestUrlError>& candidate) {
        return candidate.has_value() ||
               candidate.error() != GuestUrlError::kUnset;
      });
  if (it == std::end(candidates) || !it->has_value()) {
    return std::nullopt;
  }
  return std::move(**it);
}

bool IsGeicEnabledByFeature() {
  return base::FeatureList::IsEnabled(features::kGeic) &&
         features::kGeicEnabledParam.Get();
}

bool CanSwitchBetweenGeminiProvidersInSettings(
    content::BrowserContext* browser_context) {
  std::optional<GeicGuestUrl> resolved = ResolveGeicGuestUrl(browser_context);
  if (!resolved || resolved->source() != GuestUrlSource::kPolicy) {
    return false;
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!IsPersonalProfile(profile) ||
      !IsConsumerGeminiAllowedByPolicy(profile)) {
    return false;
  }
  const policy::PolicyMap::Entry* entry =
      GetGeminiEnterpriseUrlPolicyEntry(profile);
  return entry && entry->scope == policy::POLICY_SCOPE_MACHINE;
}

bool ShouldLatchToGeminiEnterprise(content::BrowserContext* browser_context,
                                   const GeicGuestUrl* resolved_guest_url) {
  if (!browser_context || !resolved_guest_url) {
    return false;
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || !profile->GetPrefs()) {
    return false;
  }
  if (!CanSwitchBetweenGeminiProvidersInSettings(browser_context)) {
    return true;
  }
  // Any selection other than explicit `kConsumer` (including `kAuto`,
  // `kEnterprise`, or unconfigured/corrupted values) latches to Enterprise when
  // a valid enterprise guest URL is configured.
  return profile->GetPrefs()->GetInteger(
             glic::prefs::kGlicAssistantSelection) !=
         std::to_underlying(glic::prefs::AssistantSelection::kConsumer);
}

GURL GetGeicSkillsUrl(Profile* profile) {
  const glic::GeicSpec* geic_spec =
      glic::GetGlicProviderSpec(profile).GetGeicSpec();
  const GeicGuestUrl* guest_url =
      geic_spec ? geic_spec->GetGeicGuestUrl() : nullptr;
  return guest_url ? guest_url->SkillsUrl() : GURL();
}

}  // namespace geic
