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
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "chrome/browser/platform_util.h"
#import "components/policy/core/common/policy_logger.h"
#import "net/base/apple/http_response_headers_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/http/http_request_headers.h"
#import "net/http/http_response_headers.h"
#import "net/http/http_util.h"
#import "net/url_request/url_request.h"
#import "url/gurl.h"

namespace {

// Adds the header from `headers_response` with the `headers_key` into
// `auth_headers`. `headers_key` is either "prt_headers" or "device_headers".
// `headers_response` has the following format example.
// {
//   "prt_headers": [
//     {
//       "header": {
//         "x-ms-RefreshTokenCredential": "prt_header_value"
//       },
//       "home_account_id": "123-123-1234"
//     },
//     {
//       "header": {
//         "x-ms-RefreshTokenCredential1": "prt_header_value_1"
//       },
//       "home_account_id": "abc-abc-abcd"
//     },
//     {
//       "header": {
//         "x-ms-RefreshTokenCredential2": "prt_header_value_2"
//       },
//       "home_account_id": "123-abc-12ab"
//     }
//   ],
//   "device_headers": [
//     {
//       "header": {
//         "x-ms-DeviceCredential": "device_header_value"
//       },
//       "tenant_id": "qwe-qwe-qwer"
//     }
//   ]
// }
void AddMSAuthHeadersFromSSOCookiesResponse(
    net::HttpRequestHeaders& auth_headers,
    NSDictionary* sso_cookies_response,
    NSString* headers_key) {
  static NSString* const kHeader(@"header");
  NSArray* headers = sso_cookies_response[headers_key];
  auto headers_key_str = base::SysNSStringToUTF8(headers_key);

  for (NSDictionary* header_data in headers) {
    NSDictionary* header_definition = header_data[kHeader];
    for (NSString* key in header_definition) {
      auto header_name = base::SysNSStringToUTF8(key);

      if (!net::HttpUtil::IsValidHeaderName(header_name)) {
        VLOG_POLICY(2, EXTENSIBLE_SSO)
            << "[ExtensibleEnterpriseSSO] Invalid header name "
            << headers_key_str << " : " << header_name;
        continue;
      }

      auto header_value = base::SysNSStringToUTF8(
          net::FixNSStringIncorrectlyDecodedAsLatin1(header_definition[key]));
      if (!net::HttpUtil::IsValidHeaderValue(header_value)) {
        VLOG_POLICY(2, EXTENSIBLE_SSO)
            << "[ExtensibleEnterpriseSSO] Invalid header value "
            << headers_key_str << " : " << header_value;
        continue;
      }

      VLOG_POLICY(2, EXTENSIBLE_SSO)
          << "[ExtensibleEnterpriseSSO] Header added : " << "{ " << header_name
          << ": " << header_value << "}";
      auth_headers.SetHeader(header_name, header_value);
    }
  }
}

}  // namespace

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
  ASAuthorizationController* _controller;
}

- (void)dealloc {
  // This is here for debugging purposes and will be removed once this code is
  // no longer experimental.
  if (_callback) {
    VLOG_POLICY(2, EXTENSIBLE_SSO)
        << "[ExtensibleEnterpriseSSO] Fetching headers aborted.";
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
  NSURL* nativeUrl = net::NSURLWithGURL(url);
  ASAuthorizationSingleSignOnProvider* auth_provider =
      [ASAuthorizationSingleSignOnProvider
          authorizationProviderWithIdentityProviderURL:nativeUrl];

  if (!auth_provider.canPerformAuthorization) {
    std::move(_callback).Run(net::HttpRequestHeaders());
    return;
  }
  VLOG_POLICY(2, EXTENSIBLE_SSO)
      << "[ExtensibleEnterpriseSSO] Attempting to get headers for " << url;

  // Create a request for `url`.
  ASAuthorizationSingleSignOnRequest* request = [auth_provider createRequest];

  request.requestedOperation = @"get_sso_cookies";
  if (@available(macOS 12, *)) {
    request.userInterfaceEnabled = NO;
  }

  request.authorizationOptions = @[
    [NSURLQueryItem queryItemWithName:@"sso_url"
                                value:nativeUrl.absoluteString],
    // Response headers to fetch.
    // “0” -> All headers, "1" -> PRT headers only, "2" -> Device headers only.
    [NSURLQueryItem queryItemWithName:@"types_of_header" value:@"0"],
    [NSURLQueryItem queryItemWithName:@"msg_protocol_ver" value:@"4"],
  ];

  _controller = [[ASAuthorizationController alloc]
      initWithAuthorizationRequests:[NSArray arrayWithObject:request]];
  _controller.delegate = self;
  _controller.presentationContextProvider = self;

  [_controller performRequests];
}

// ASAuthorizationControllerDelegate implementation

// Called when the authentication was successful and creates a
// HttpRequestHeaders from `authorization`.
- (void)authorizationController:(ASAuthorizationController*)controller
    didCompleteWithAuthorization:(ASAuthorization*)authorization {
  static NSString* const kSSOCookies(@"sso_cookies");
  static NSString* const kPrtHeaders(@"prt_headers");
  static NSString* const kDeviceHeaders(@"device_headers");

  VLOG_POLICY(2, EXTENSIBLE_SSO)
      << "[ExtensibleEnterpriseSSO] Fetching headers completed.";
  ASAuthorizationSingleSignOnCredential* credential = authorization.credential;
  // An example response headers:
  // {
  //   "sso_cookies": "JSON formated object"
  //   "operation": "get_sso_cookies",
  //   "broker_version": "3.3.23",
  //   "preferred_auth_config": "preferredAuthNotConfigured",
  //   "success": "1",
  //   "wpj_status": "notJoined",
  //   "operation_response_type": "operation_get_sso_cookies_response",
  //   "request_received_timestamp": "1736954944.2245578766",
  //   "extraDeviceInfo": "{}",
  //   "sso_extension_mode": "full",
  //   "device_mode": "personal",
  //   "response_gen_timestamp": "1736954944.7778768539",
  //   "platform_sso_status": "platformSSONotEnabled"
  // }
  NSDictionary* all_headers = credential.authenticatedResponse.allHeaderFields;
  // This is for logging purposes only.
  {
    NSString* all_headers_json = [[NSString alloc]
        initWithData:[NSJSONSerialization dataWithJSONObject:all_headers
                                                     options:0
                                                       error:nil]
            encoding:NSUTF8StringEncoding];
    VLOG_POLICY(2, EXTENSIBLE_SSO) << "[ExtensibleEnterpriseSSO] Headers: "
                                   << base::SysNSStringToUTF8(all_headers_json);
  }
  NSString* sso_cookies_json = all_headers[kSSOCookies];
  VLOG_POLICY(2, EXTENSIBLE_SSO) << "[ExtensibleEnterpriseSSO] SSO Cookies: "
                                 << base::SysNSStringToUTF8(sso_cookies_json);

  net::HttpRequestHeaders auth_headers;
  NSDictionary* sso_cookies = [NSJSONSerialization
      JSONObjectWithData:[sso_cookies_json
                             dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  if (!sso_cookies) {
    VLOG_POLICY(2, EXTENSIBLE_SSO)
        << "[ExtensibleEnterpriseSSO] Failed to deserialize sso_cookies";
    std::move(_callback).Run(std::move(auth_headers));
    return;
  }

  AddMSAuthHeadersFromSSOCookiesResponse(auth_headers, sso_cookies,
                                         kPrtHeaders);
  AddMSAuthHeadersFromSSOCookiesResponse(auth_headers, sso_cookies,
                                         kDeviceHeaders);

  std::move(_callback).Run(std::move(auth_headers));
}

// Called when the authentication failed and creates a
// empty HttpRequestHeaders.
- (void)authorizationController:(ASAuthorizationController*)controller
           didCompleteWithError:(NSError*)error {
  VLOG_POLICY(2, EXTENSIBLE_SSO)
      << "[ExtensibleEnterpriseSSO] Fetching headers failed";
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

namespace {

// Empty function used to ensure SSOServiceAuthControllerDelegate does not get
// destroyed until the data is fetched.
void OnDataFetched(SSOServiceAuthControllerDelegate*) {
  VLOG_POLICY(2, EXTENSIBLE_SSO)
      << "[ExtensibleEnterpriseSSO] Deleting SSOServiceAuthControllerDelegate";
}

}  // namespace

ExtensibleEnterpriseSSOProvider::ExtensibleEnterpriseSSOProvider() = default;

ExtensibleEnterpriseSSOProvider::~ExtensibleEnterpriseSSOProvider() = default;

bool ExtensibleEnterpriseSSOProvider::SupportsOriginFiltering() {
  return false;
}

void ExtensibleEnterpriseSSOProvider::FetchOrigins(
    FetchOriginsCallback on_fetch_complete) {
  // Origin filtering is nor supported.
  NOTREACHED();
}

void ExtensibleEnterpriseSSOProvider::GetData(
    const GURL& url,
    PlatformAuthProviderManager::GetDataCallback callback) {
  SSOServiceAuthControllerDelegate* delegate =
      [[SSOServiceAuthControllerDelegate alloc] init];
  auto final_callback = base::BindPostTaskToCurrentDefault(
      std::move(callback).Then(base::BindOnce(&OnDataFetched, delegate)));
  [delegate getAuthHeaders:url withCallback:std::move(final_callback)];
}

}  // namespace enterprise_auth
