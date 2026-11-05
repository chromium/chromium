// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/google_groups_manager.h"
#include "content/public/browser/browser_context.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace geic {

namespace {

// Although `GeminiEnterpriseUrlPolicyHandler` validates the policy URL's
// syntax, scheme and host when applying the enterprise policy, we validate the
// URL here as well so that all guest URL sources (`--geic-guest-url` switch,
// policy pref, and `features::kGeicGuestURL` Finch param) and direct pref
// writes in tests are checked before the origin is granted Glic Mojo API
// access. Note that `IsAllowedGeminiEnterpriseHost` checks production hosts
// and the `features::kGeicGuestURL` Finch host, while `--glic-dev` is accepted
// in `IsValidGuestUrl`.
bool IsValidGuestUrl(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  if (url.SchemeIsHTTPOrHTTPS() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(::switches::kGlicDev)) {
    return true;
  }
  return url.SchemeIs(url::kHttpsScheme) && IsAllowedGeminiEnterpriseHost(url);
}

// Rewrites standalone Gemini Enterprise web app URLs (`/home/cid/<configId>` or
// `/<location>/home/cid/<configId>`) to the side-panel endpoint
// (`/side-panel?configId=<configId>` or
// `/side-panel?configId=<configId>&location=<location>`) at consumption time
// rather than in the policy handler so the stored policy pref retains the raw
// web application URL and CLI/Finch overrides are canonicalized as well.
GURL CanonicalizeGuestUrl(const GURL& input_url) {
  if (!input_url.is_valid()) {
    return GURL();
  }

  static constexpr std::string_view kHomeCidMarker = "/home/cid/";
  std::string_view path = input_url.path();
  std::string_view location;
  std::string_view config_id;

  size_t marker_pos = path.find(kHomeCidMarker);
  if (marker_pos == 0) {
    config_id = path.substr(kHomeCidMarker.size());
  } else if (marker_pos != std::string_view::npos && marker_pos > 1) {
    // Expected path: "/<location>/home/cid/..."
    std::string_view prefix = path.substr(1, marker_pos - 1);
    if (prefix.find('/') == std::string_view::npos) {
      location = prefix;
      config_id = path.substr(marker_pos + kHomeCidMarker.size());
    }
  }

  if (config_id.ends_with("/")) {
    config_id.remove_suffix(1);
  }
  if (config_id.empty()) {
    return input_url;
  }

  GURL::Replacements replacements;
  replacements.SetPathStr("/side-panel");
  GURL result = net::AppendOrReplaceQueryParameter(
      input_url.ReplaceComponents(replacements), "configId", config_id);
  if (!location.empty()) {
    result = net::AppendOrReplaceQueryParameter(result, "location", location);
  }
  return result;
}

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
             std::to_underlying(
                 optimization_guide::prefs::GeminiSettingsPolicyState::
                     kEnabled);
}

bool IsPersonalProfile(Profile* profile) {
  return profile && !profile->IsOffTheRecord() &&
         profile->GetProfilePolicyConnector() &&
         !profile->GetProfilePolicyConnector()->IsManaged();
}

// Which configuration source produced the resolved guest URL.
enum class GuestUrlSource {
  kSwitch,
  kPolicy,
  kFinch,
};

struct ResolvedGuestUrl {
  GURL url;
  GuestUrlSource source;
};

// Why a guest URL source didn't produce a usable URL.
enum class GuestUrlError {
  // The source is not set.
  kUnset,
  // The source is set, but to an unparsable or disallowed URL.
  kInvalid,
};

// Classifies a guest URL source. Returns the validated URL, ready to be
// canonicalized, or why it can't be used.
base::expected<GURL, GuestUrlError> ToGuestUrl(const std::string& spec) {
  if (spec.empty()) {
    return base::unexpected(GuestUrlError::kUnset);
  }
  GURL url(spec);
  if (!IsValidGuestUrl(url)) {
    return base::unexpected(GuestUrlError::kInvalid);
  }
  return url;
}

base::expected<GURL, GuestUrlError> GetPolicyGuestUrl(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile && profile->GetPrefs()
             ? ToGuestUrl(profile->GetPrefs()->GetString(
                   glic::prefs::kGlicGeminiEnterpriseUrl))
             : base::unexpected(GuestUrlError::kUnset);
}

base::expected<GURL, GuestUrlError> GetFinchGuestUrl(
    content::BrowserContext* browser_context) {
  if (browser_context) {
    const GoogleGroupsManager* google_groups_manager =
        GoogleGroupsManagerFactory::GetForBrowserContext(browser_context);
    if (google_groups_manager &&
        !google_groups_manager->IsFeatureEnabledForProfile(features::kGeic)) {
      return base::unexpected(GuestUrlError::kUnset);
    }
  }
  return ToGuestUrl(features::kGeicGuestURL.Get());
}

std::optional<ResolvedGuestUrl> ResolveGuestUrlWithSource(
    content::BrowserContext* browser_context) {
  if (!IsGeicEnabledByFeature()) {
    return std::nullopt;
  }

  // `--geic-guest-url` is evaluated before the enterprise policy to simplify
  // development and local testing; `IsValidGuestUrl()` still restricts the URL
  // to valid Gemini Enterprise origins and GEiC requires signing in to GE.
  const std::pair<GuestUrlSource, base::expected<GURL, GuestUrlError>>
      candidates[] = {
          {GuestUrlSource::kSwitch,
           ToGuestUrl(
               base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                   kGeicGuestURLSwitch))},
          {GuestUrlSource::kPolicy, GetPolicyGuestUrl(browser_context)},
          {GuestUrlSource::kFinch, GetFinchGuestUrl(browser_context)},
      };
  // The first set candidate wins, even if it is invalid, so that an invalid
  // higher-priority source doesn't silently fall through.
  auto it = std::ranges::find_if(candidates, [](const auto& entry) {
    return entry.second.has_value() ||
           entry.second.error() != GuestUrlError::kUnset;
  });
  if (it == std::end(candidates) || !it->second.has_value()) {
    return std::nullopt;
  }
  return ResolvedGuestUrl{CanonicalizeGuestUrl(it->second.value()), it->first};
}

}  // namespace

bool IsGeicEnabledByFeature() {
  return base::FeatureList::IsEnabled(features::kGeic) &&
         features::kGeicEnabledParam.Get();
}

bool CanSwitchBetweenGeminiProvidersInSettings(
    content::BrowserContext* browser_context) {
  std::optional<ResolvedGuestUrl> resolved =
      ResolveGuestUrlWithSource(browser_context);
  if (!resolved || resolved->source != GuestUrlSource::kPolicy) {
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

GURL ResolveGeicGuestUrl(content::BrowserContext* browser_context) {
  std::optional<ResolvedGuestUrl> resolved =
      ResolveGuestUrlWithSource(browser_context);
  return resolved ? std::move(resolved->url) : GURL();
}

bool ShouldLatchToGeminiEnterprise(content::BrowserContext* browser_context,
                                   const GURL& resolved_guest_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (resolved_guest_url.is_empty() || !profile || !profile->GetPrefs()) {
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
  GURL url = glic::GetGlicProviderSpec(profile).GetGuestUrl(profile);
  if (!url.is_valid()) {
    return GURL();
  }

  std::string config_id;
  net::GetValueForKeyInQuery(url, "configId", &config_id);
  url = net::AppendOrReplaceQueryParameter(url, "configId", std::nullopt);
  url = net::AppendOrReplaceQueryParameter(url, "location", std::nullopt);
  url = net::AppendOrReplaceQueryParameter(url, "hl", std::nullopt);

  std::string path =
      config_id.empty()
          ? "/r/skills"
          : base::StrCat({"/home/cid/", config_id, "/r/skills"});
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  if (url.query().empty()) {
    replacements.ClearQuery();
  }
  return url.ReplaceComponents(replacements);
}

}  // namespace geic
