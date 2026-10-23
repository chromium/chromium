// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/types/expected.h"
#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
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

// Rewrites standalone Gemini Enterprise web app URLs (`/home/cid/<configId>`)
// to the side-panel endpoint (`/side-panel?configId=<configId>`) at consumption
// time rather than in the policy handler so the stored policy pref retains the
// raw web application URL and CLI/Finch overrides are canonicalized as well.
GURL CanonicalizeGuestUrl(const GURL& input_url) {
  if (!input_url.is_valid()) {
    return GURL();
  }

  static constexpr std::string_view kHomeCidPrefix = "/home/cid/";
  std::string_view path = input_url.path();
  if (!path.starts_with(kHomeCidPrefix)) {
    return input_url;
  }
  path.remove_prefix(kHomeCidPrefix.size());
  if (path.ends_with("/")) {
    path.remove_suffix(1);
  }
  if (path.empty()) {
    return input_url;
  }

  GURL::Replacements replacements;
  replacements.SetPathStr("/side-panel");
  return net::AppendOrReplaceQueryParameter(
      input_url.ReplaceComponents(replacements), "configId", path);
}

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

}  // namespace

bool IsGeicEnabledByFeature() {
  return base::FeatureList::IsEnabled(features::kGeic) &&
         features::kGeicEnabledParam.Get();
}

GURL ResolveGeicGuestUrl(content::BrowserContext* browser_context) {
  if (!IsGeicEnabledByFeature()) {
    return GURL();
  }

  const base::expected<GURL, GuestUrlError> candidates[] = {
      ToGuestUrl(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kGeicGuestURLSwitch)),
      GetPolicyGuestUrl(browser_context),
      GetFinchGuestUrl(browser_context),
  };
  // The first set candidate wins, even if it is invalid, so that an invalid
  // higher-priority source doesn't silently fall through.
  auto it = std::ranges::find_if(
      candidates, [](const base::expected<GURL, GuestUrlError>& candidate) {
        return candidate.has_value() ||
               candidate.error() != GuestUrlError::kUnset;
      });
  return it != std::end(candidates) && it->has_value()
             ? CanonicalizeGuestUrl(it->value())
             : GURL();
}

}  // namespace geic
