// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_ENTRA_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_ENTRA_H_

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"
#import "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"

namespace net {
class HttpRequestHeaders;
}

// Interface that provides a presentation context to the platform
// and a delegate for the authorization controller.
@interface SSOServiceEntraAuthControllerDelegate
    : NSObject <ASAuthorizationControllerDelegate,
                ASAuthorizationControllerPresentationContextProviding>

- (ASAuthorizationSingleSignOnRequest*)createRequest;

- (void)performRequest;

- (ASAuthorizationController*)createAuthorizationControllerWithRequest:
    (ASAuthorizationSingleSignOnRequest*)request;

// Gets authentication headers for `url` if the device can perform
// authentication for it.
// If the device can perform the authentication, `withCallback` is called
// with headers built from the response from the device, otherwise it is called
// with empty headers.
- (void)getAuthHeaders:(NSURL*)url
          withCallback:
              (base::OnceCallback<
                  void(std::unique_ptr<
                       enterprise_auth::ExtensibleEnterpriseSSOProvider::
                           DelegateResult>)>)callback;

- (net::HttpRequestHeaders)getHeadersFromHttpResponse:
    (NSHTTPURLResponse*)response;

- (instancetype)initWithAuthorizationSingleSignOnProvider:
    (ASAuthorizationSingleSignOnProvider*)auth_provider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_ENTRA_H_
