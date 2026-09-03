// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_enabling.h"

#include <string_view>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace geic {

bool IsGeicEnabled(Profile* profile) {
  if (!profile || !profile->IsRegularProfile()) {
    return false;
  }
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kGeicEnabled) &&
      !base::FeatureList::IsEnabled(features::kGeic)) {
    return false;
  }
  const auto channel = chrome::GetChannel();
  const bool is_developer_build = !version_info::IsOfficialBuild() ||
                                  channel == version_info::Channel::UNKNOWN ||
                                  channel == version_info::Channel::CANARY;
  // TODO(crbug.com/545155625): Remove --disable-web-security requirement before
  // teamfood. Currently required so users see a security warning banner if led
  // to enabling this via social engineering.
  if (!is_developer_build &&
      !command_line->HasSwitch(::switches::kDisableWebSecurity)) {
    return false;
  }
  return !base::FeatureList::IsEnabled(features::kGlic);
}

bool IsValidGuestUrl(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  // Allow localhost for local development and testing.
  if (net::IsLocalhost(url)) {
    return true;
  }
  if (!url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  const std::string_view host = url.host();
  return host == "business.gemini.google" || host == "gemini.google.com" ||
         url.DomainIs("cloud.google.com") || url.DomainIs("corp.google.com");
}

GURL CanonicalizeGuestUrl(const GURL& input_url) {
  if (!input_url.is_valid()) {
    return GURL();
  }

  // If the path starts with "/home/cid/<configId>", canonicalize it to
  // "/side-panel?configId=<configId>".
  static constexpr std::string_view kHomeCidPrefix = "/home/cid/";
  std::string_view path = input_url.path();
  if (path.starts_with(kHomeCidPrefix)) {
    path.remove_prefix(kHomeCidPrefix.size());
    if (path.ends_with("/")) {
      path.remove_suffix(1);
    }
    if (!path.empty()) {
      GURL::Replacements replacements;
      replacements.SetPathStr("/side-panel");
      std::string new_query = base::StrCat({"configId=", path});
      if (input_url.has_query()) {
        base::StrAppend(&new_query, {"&", input_url.query()});
      }
      replacements.SetQueryStr(new_query);
      return input_url.ReplaceComponents(replacements);
    }
  }

  return input_url;
}

GURL GetPolicyGuestUrl(Profile* profile) {
  if (!profile || !profile->GetPrefs()) {
    return GURL();
  }
  const base::DictValue& dict =
      profile->GetPrefs()->GetDict("glic.gemini_enterprise_settings");
  const std::string* url_str = dict.FindString("url");
  if (url_str && !url_str->empty()) {
    GURL url(*url_str);
    if (IsValidGuestUrl(url)) {
      return CanonicalizeGuestUrl(url);
    }
  }
  return GURL();
}

}  // namespace geic
