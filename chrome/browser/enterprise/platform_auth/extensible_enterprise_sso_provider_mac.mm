// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#import <string>
#import <utility>
#import <vector>

#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "chrome/browser/platform_util.h"
#import "net/base/apple/http_response_headers_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/http/http_request_headers.h"
#import "net/http/http_response_headers.h"
#import "net/http/http_util.h"
#import "net/url_request/url_request.h"
#import "url/gurl.h"

// Interface that provides a presentation context to the platform
// and a delegate for the authorization controller.
@interface SSOServiceAuthControllerDelegate
    : NSObject <ASAuthorizationControllerDelegate,
                ASAuthorizationControllerPresentationContextProviding>
@end

// Class that allows fetching authentication headers for a url if it is
// supported by any SSO extension on the device.
@implementation SSOServiceAuthControllerDelegate {
  enterprise_auth::PlatformAuthProviderManager::GetDataCallback _callback;
}

- (void)dealloc {
  VLOG(1) << "[ExtensibleEnterpriseSSO] Destroying "
             "SSOServiceAuthControllerDelegate";
  if (_callback) {
    VLOG(1) << "[ExtensibleEnterpriseSSO] Fetching headers aborted.";
    std::move(_callback).Run(net::HttpRequestHeaders());
  }
}

// Gets authentication headers for `url` if the device can perform
// authentication for it.
// If the device can perform the authentication, `withCallback` is called
// with headers built from the response from the device, otherwise it is called
// with empty headers.
- (void)getAuthHeaders:(GURL)url
          withCallback:
              (enterprise_auth::PlatformAuthProviderManager::GetDataCallback)
                  callback {
  _callback = std::move(callback);
  VLOG(1) << "[ExtensibleEnterpriseSSO] Attempting to get headers for " << url;
  ASAuthorizationSingleSignOnProvider* auth_provider =
      [ASAuthorizationSingleSignOnProvider
          authorizationProviderWithIdentityProviderURL:net::NSURLWithGURL(url)];

  if (!auth_provider.canPerformAuthorization) {
    VLOG(1) << "[ExtensibleEnterpriseSSO] Fetching headers for not supported.";
    std::move(_callback).Run(net::HttpRequestHeaders());
    return;
  }

  // Create a login request for `url`.
  ASAuthorizationSingleSignOnRequest* request = [auth_provider createRequest];
  request.requestedOperation = ASAuthorizationOperationLogin;
  ASAuthorizationController* controller = [[ASAuthorizationController alloc]
      initWithAuthorizationRequests:[NSArray arrayWithObject:request]];
  controller.delegate = self;
  controller.presentationContextProvider = self;

  [controller performRequests];
}

// ASAuthorizationControllerDelegate implementation

// Called when the authentication was successful and creates a
// HttpRequestHeaders from `authorization`.
- (void)authorizationController:(ASAuthorizationController*)controller
    didCompleteWithAuthorization:(ASAuthorization*)authorization {
  VLOG(1) << "[ExtensibleEnterpriseSSO] Fetching headers completed.";
  ASAuthorizationSingleSignOnRequest* request =
      (ASAuthorizationSingleSignOnRequest*)
          controller.authorizationRequests.firstObject;
  if (!request || request.requestedOperation != ASAuthorizationOperationLogin) {
    VLOG(1)
        << "[ExtensibleEnterpriseSSO] Fetching headers completed for non-login "
           "operation.";
    std::move(_callback).Run(net::HttpRequestHeaders());
    return;
  }
  VLOG(1) << "[ExtensibleEnterpriseSSO] Fetching headers completed for login "
             "operation.";
  ASAuthorizationSingleSignOnCredential* credential = authorization.credential;
  NSDictionary* headers = credential.authenticatedResponse.allHeaderFields;
  net::HttpRequestHeaders request_headers;
  for (NSString* key in headers) {
    const std::string header_name = base::SysNSStringToUTF8(key);
    if (!net::HttpUtil::IsValidHeaderName(header_name)) {
      VLOG(1) << "[ExtensibleEnterpriseSSO] Invalid header name "
              << header_name;
      continue;
    }

    const std::string header_value = base::SysNSStringToUTF8(
        net::FixNSStringIncorrectlyDecodedAsLatin1(headers[key]));
    if (!net::HttpUtil::IsValidHeaderValue(header_value)) {
      VLOG(1) << "[ExtensibleEnterpriseSSO] Invalid header value "
              << header_value;
      continue;
    }

    request_headers.SetHeader(header_name, header_value);
  }
  std::move(_callback).Run(std::move(request_headers));
}

// Called when the authentication failed and creates a
// empty HttpRequestHeaders.
- (void)authorizationController:(ASAuthorizationController*)controller
           didCompleteWithError:(NSError*)error {
  VLOG(1) << "[ExtensibleEnterpriseSSO] Fetching headers failed";
  std::move(_callback).Run(net::HttpRequestHeaders());
}

// ASAuthorizationControllerPresentationContextProviding implementation
- (ASPresentationAnchor)presentationAnchorForAuthorizationController:
    (ASAuthorizationController*)controller {
  // TODO(b/340868357): Pick the window where the url is being used.
  return platform_util::GetActiveWindow();
}

@end

namespace enterprise_auth {

bool ExtensibleEnterpriseSSOProvider::SupportsOriginFiltering() {
  return false;
}

void ExtensibleEnterpriseSSOProvider::FetchOrigins(
    FetchOriginsCallback on_fetch_complete) {
  // Origin filtering is nor supported.
  NOTREACHED_NORETURN();
}

void ExtensibleEnterpriseSSOProvider::GetData(
    const GURL& url,
    PlatformAuthProviderManager::GetDataCallback callback) {
  SSOServiceAuthControllerDelegate* delegate =
      [[SSOServiceAuthControllerDelegate alloc] init];
  [delegate
      getAuthHeaders:url
        withCallback:base::BindPostTaskToCurrentDefault(std::move(callback))];
}

}  // namespace enterprise_auth
