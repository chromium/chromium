// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"

#import <Foundation/Foundation.h>

#import <string>
#import <utility>
#import <vector>

#import "base/functional/callback.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "chrome/browser/browser_process.h"
#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_entra.h"
#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_policy_handler.h"
#import "chrome/common/pref_names.h"
#import "components/policy/core/common/policy_logger.h"
#import "components/prefs/pref_service.h"
#import "net/base/apple/http_response_headers_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/http/http_request_headers.h"
#import "net/http/http_response_headers.h"
#import "net/http/http_util.h"
#import "net/url_request/url_request.h"
#import "url/gurl.h"

namespace enterprise_auth {

namespace {

constexpr std::array<const char*, 1> kSupportedIdps{
    kMicrosoftIdentityProvider,
};

// Empty function used to ensure SSOServiceEntraAuthControllerDelegate does not
// get destroyed until the data is fetched.
void OnDataFetched(SSOServiceEntraAuthControllerDelegate*) {
  VLOG_POLICY(2, EXTENSIBLE_SSO) << "[ExtensibleEnterpriseSSO] Deleting "
                                    "SSOServiceEntraAuthControllerDelegate";
}

}  // namespace

ExtensibleEnterpriseSSOProvider::ExtensibleEnterpriseSSOProvider() = default;

ExtensibleEnterpriseSSOProvider::~ExtensibleEnterpriseSSOProvider() = default;

bool ExtensibleEnterpriseSSOProvider::SupportsOriginFiltering() {
  return false;
}

void ExtensibleEnterpriseSSOProvider::FetchOrigins(
    FetchOriginsCallback on_fetch_complete) {
  // Origin filtering is not supported.
  NOTREACHED();
}

void ExtensibleEnterpriseSSOProvider::GetData(
    const GURL& url,
    PlatformAuthProviderManager::GetDataCallback callback) {
  NSURL* nativeUrl = net::NSURLWithGURL(url);
  ASAuthorizationSingleSignOnProvider* auth_provider =
      [ASAuthorizationSingleSignOnProvider
          authorizationProviderWithIdentityProviderURL:nativeUrl];
  if (!auth_provider.canPerformAuthorization) {
    std::move(callback).Run(net::HttpRequestHeaders());
    return;
  }

  for (const base::Value& idp_value : g_browser_process->local_state()->GetList(
           prefs::kExtensibleEnterpriseSSOEnabledIdps)) {
    if (const std::string* idp = idp_value.GetIfString();
        idp && *idp == kMicrosoftIdentityProvider) {
      SSOServiceEntraAuthControllerDelegate* delegate =
          [[SSOServiceEntraAuthControllerDelegate alloc]
              initWithAuthorizationSingleSignOnProvider:auth_provider];

      // Pass `delegate` as a callback parameter so that it lives beyond the
      // scope of this function and until the callback is called.
      auto final_callback = base::BindPostTaskToCurrentDefault(
          std::move(callback).Then(base::BindOnce(&OnDataFetched, delegate)));
      [delegate getAuthHeaders:nativeUrl
                  withCallback:std::move(final_callback)];
    }
  }
}

// static
std::set<std::string>
ExtensibleEnterpriseSSOProvider::GetSupportedIdentityProviders() {
  return {kSupportedIdps.begin(), kSupportedIdps.end()};
}

// static
base::Value::List
ExtensibleEnterpriseSSOProvider::GetSupportedIdentityProvidersList() {
  base::Value::List idps;
  for (const char* idp : kSupportedIdps) {
    idps.Append(idp);
  }
  return idps;
}

}  // namespace enterprise_auth
