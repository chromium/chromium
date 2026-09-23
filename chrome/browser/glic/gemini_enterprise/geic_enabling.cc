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
#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace geic {

namespace {

// Although `GeminiEnterpriseSettingsPolicyHandler` validates URL syntax and
// HTTPS scheme when applying the enterprise policy, we validate the URL and
// host allowlist here as well so that all guest URL sources (`--geic-guest-url`
// switch, policy pref, and `features::kGeicGuestURL` Finch param) and direct
// pref writes in tests are checked before the origin is granted Glic Mojo API
// access.
bool IsValidGuestUrl(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  if (url.SchemeIsHTTPOrHTTPS() && net::IsLocalhost(url)) {
    return true;
  }
  if (!url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  const std::string_view host = url.host();
  return host == "business.gemini.google" || host == "gemini.google.com" ||
         url.DomainIs("cloud.google.com") || url.DomainIs("cloud.google") ||
         url.DomainIs("corp.google.com");
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

std::string GetPolicyGuestUrl(Profile* profile) {
  if (!profile || !profile->GetPrefs()) {
    return std::string();
  }
  const base::DictValue& dict =
      profile->GetPrefs()->GetDict(glic::prefs::kGlicGeminiEnterpriseSettings);
  const std::string* url_str = dict.FindString("url");
  return url_str ? *url_str : std::string();
}

}  // namespace

bool IsGeicEnabled() {
  if (!base::FeatureList::IsEnabled(features::kGeic)) {
    return false;
  }

  return features::kGeicEnabledParam.Get();
}

GURL GetGeicGuestUrl(Profile* profile) {
  if (!IsGeicEnabled()) {
    return GURL();
  }

  const std::string candidates[] = {
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kGeicGuestURLSwitch),
      GetPolicyGuestUrl(profile),
      features::kGeicGuestURL.Get(),
  };
  auto it = std::ranges::find_if(
      candidates, [](const std::string& s) { return !s.empty(); });
  if (it != std::end(candidates)) {
    GURL url(*it);
    if (IsValidGuestUrl(url)) {
      return CanonicalizeGuestUrl(url);
    }
  }
  return GURL();
}

}  // namespace geic
