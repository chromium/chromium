// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/glic/gemini_enterprise/geic_guest_url.h"
#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
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

bool IsRegularUserProfile(content::BrowserContext* browser_context) {
  if (!browser_context) {
    return false;
  }
  static const base::NoDestructor<ProfileSelections> kRegularProfiles(
      ProfileSelections::BuildForRegularProfile());
  return kRegularProfiles->ApplyProfileSelection(
             Profile::FromBrowserContext(browser_context)) != nullptr;
}

bool IsGeicFinchGroupEnabledForContext(
    content::BrowserContext* browser_context) {
  if (!IsRegularUserProfile(browser_context)) {
    return false;
  }
  const GoogleGroupsManager* google_groups_manager =
      GoogleGroupsManagerFactory::GetForBrowserContext(browser_context);
  return !google_groups_manager ||
         google_groups_manager->IsFeatureEnabledForProfile(features::kGeic);
}

std::vector<GeicSelectableStage> ParseSelectableFinchStages() {
  const std::string raw_json = features::kGeicSelectableStages.Get();
  if (raw_json.empty()) {
    return {};
  }
  std::optional<base::ListValue> list =
      base::JSONReader::ReadList(raw_json, base::JSON_PARSE_RFC);
  if (!list) {
    return {};
  }

  std::vector<GeicSelectableStage> stages;
  std::set<std::string> seen_ids;
  for (const base::Value& entry : *list) {
    const base::DictValue* dict = entry.GetIfDict();
    if (!dict) {
      continue;
    }
    const std::string* id = dict->FindString("id");
    const std::string* label = dict->FindString("label");
    const std::string* url_spec = dict->FindString("url");
    if (!id || id->empty() || !label || label->empty() || !url_spec ||
        url_spec->empty()) {
      continue;
    }
    base::expected<GeicGuestUrl, GuestUrlError> guest_url =
        GeicGuestUrl::FromFinch(*url_spec);
    if (!guest_url.has_value()) {
      continue;
    }
    if (!seen_ids.insert(*id).second) {
      continue;
    }
    stages.push_back(GeicSelectableStage{
        .id = *id,
        .label = *label,
        .guest_url = std::move(*guest_url),
    });
  }
  return stages;
}

base::expected<GeicGuestUrl, GuestUrlError> GetFinchGuestUrl(
    content::BrowserContext* browser_context) {
  if (!IsGeicFinchGroupEnabledForContext(browser_context)) {
    return base::unexpected(GuestUrlError::kUnset);
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile && profile->GetPrefs()) {
    const std::string& selected_stage_id =
        profile->GetPrefs()->GetString(prefs::kGeicSelectedStageId);
    if (!selected_stage_id.empty()) {
      std::vector<GeicSelectableStage> stages = ParseSelectableFinchStages();
      auto it = std::ranges::find_if(
          stages, [&selected_stage_id](const GeicSelectableStage& stage) {
            return stage.id == selected_stage_id;
          });
      if (it != stages.end()) {
        return std::move(it->guest_url);
      }
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

std::vector<GeicSelectableStage> GetSelectableRolloutStages(Profile* profile) {
  if (!IsRegularUserProfile(profile) || !profile->GetPrefs() ||
      !IsGeicEnabledByFeature()) {
    return {};
  }
  const base::expected<GeicGuestUrl, GuestUrlError> switch_url =
      GetSwitchGuestUrl();
  if (switch_url.has_value() || switch_url.error() != GuestUrlError::kUnset) {
    return {};
  }
  const base::expected<GeicGuestUrl, GuestUrlError> policy_url =
      GetPolicyGuestUrl(profile);
  if (policy_url.has_value() || policy_url.error() != GuestUrlError::kUnset) {
    return {};
  }
  if (!IsGeicFinchGroupEnabledForContext(profile)) {
    return {};
  }
  return ParseSelectableFinchStages();
}

std::string GetCurrentRolloutStageId(
    Profile* profile,
    base::span<const GeicSelectableStage> stages) {
  if (!IsRegularUserProfile(profile) || stages.empty()) {
    return std::string();
  }

  auto find_stage_by_url =
      [&stages](const GeicGuestUrl* guest_url) -> std::string {
    if (!guest_url) {
      return std::string();
    }
    auto it = std::ranges::find_if(
        stages, [guest_url](const GeicSelectableStage& stage) {
          return stage.guest_url == *guest_url;
        });
    return it != stages.end() ? it->id : std::string();
  };

  if (const glic::GlicProviderLatch* latch =
          glic::GlicProviderLatch::GetIfLatched(*profile)) {
    const glic::GeicSpec* geic_spec = latch->GetSpec().GetGeicSpec();
    return find_stage_by_url(geic_spec ? geic_spec->GetGeicGuestUrl()
                                       : nullptr);
  }

  std::optional<GeicGuestUrl> resolved = ResolveGeicGuestUrl(profile);
  return find_stage_by_url(resolved ? &*resolved : nullptr);
}

}  // namespace geic
