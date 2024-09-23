// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/login_detection_util.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/scheme_host_port.h"
#include "url/url_canon.h"

namespace login_detection {

BASE_FEATURE(kLoginDetection,
             "LoginDetection",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsLoginDetectionFeatureEnabled() {
  return base::FeatureList::IsEnabled(kLoginDetection);
}

std::string GetSiteNameForURL(const GURL& url) {
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  std::string scheme = url.scheme();
  // Use the default port for the scheme to ignore any non-standard ports for
  // the scheme from the final string being returned. So,
  // https://www.foo.com:1000/page.html would return just https://foo.com
  return url::SchemeHostPort(scheme, domain.empty() ? url.host() : domain,
                             url::DefaultPortForScheme(scheme))
      .Serialize();
}

std::set<std::string> GetOAuthLoginStartQueryParams() {
  std::string param = GetFieldTrialParamValueByFeature(
      kLoginDetection, "oauth_login_start_request_params");
  if (param.empty())
    param = "client_id";
  auto params = base::SplitString(param, ",", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY);
  return std::set<std::string>(params.begin(), params.end());
}

std::set<std::string> GetOAuthLoginCompleteQueryParams() {
  std::string param = GetFieldTrialParamValueByFeature(
      kLoginDetection, "oauth_login_complete_request_params");
  if (param.empty())
    param = "code";
  auto params = base::SplitString(param, ",", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY);
  return std::set<std::string>(params.begin(), params.end());
}

size_t GetOAuthLoginFlowStartToCompleteLimit() {
  // By default allow 4 navigations (including the OAuth start navigation)
  // before an OAuth completion is detected. This allows some leeway for the
  // user to type-in password and login to the OAuth provider.
  return GetFieldTrialParamByFeatureAsInt(
      kLoginDetection, "oauth_login_start_to_complete_limit", 4);
}

size_t GetOauthLoggedInSitesMaxSize() {
  return GetFieldTrialParamByFeatureAsInt(kLoginDetection,
                                          "oauth_loggedin_sites_max_size", 100);
}

std::vector<std::string> GetLoggedInSitesFromFieldTrial() {
  // Get the field trial parameter which is a list of comma separated sites.
  std::string param =
      GetFieldTrialParamValueByFeature(kLoginDetection, "logged_in_sites");
  if (param.empty())
    return std::vector<std::string>();
  return base::SplitString(param, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

}  // namespace login_detection
