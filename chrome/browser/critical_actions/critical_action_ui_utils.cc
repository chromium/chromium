// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/critical_action_ui_utils.h"

#include "base/strings/strcat.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace critical_actions {

namespace {

std::string GetPasswordManagerLinkoutUrl(const GURL& url) {
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty()) {
    domain = std::string(url.host());
  }
  if (!domain.empty()) {
    return base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/", domain});
  }
  return chrome::kChromeUIPasswordManagerURL;
}

std::string GetSiteDetailsLinkoutUrl(const GURL& page_url) {
  const url::Origin origin = url::Origin::Create(page_url);
  // An opaque origin serialises to the literal string "null", which would
  // produce a site details page for a site that does not exist. Fall back to
  // the settings root instead.
  if (origin.opaque()) {
    return chrome::kChromeUISettingsURL;
  }
  return net::AppendQueryParameter(
             GURL(base::StrCat(
                 {chrome::kChromeUISettingsURL, chrome::kSiteDetailsSubpage})),
             "site", origin.Serialize())
      .spec();
}

}  // namespace

std::string GetCriticalActionLinkoutUrl(ActionType action_type,
                                        const GURL& page_url) {
  switch (action_type) {
    case ActionType::kCredentialAccess:
    case ActionType::kGooglePasswordManager:
    case ActionType::kFederatedLogin:
    case ActionType::kCredentialsOtp:
      return GetPasswordManagerLinkoutUrl(page_url);
    case ActionType::kFormFill:
      return base::StrCat(
          {chrome::kChromeUISettingsURL, chrome::kAddressesSubPage});
    case ActionType::kDownload:
      return chrome::kChromeUIDownloadsURL;
    case ActionType::kSettingChange:
      return GetSiteDetailsLinkoutUrl(page_url);
    case ActionType::kUnknown:
      return chrome::kChromeUISettingsURL;
  }
}

}  // namespace critical_actions
