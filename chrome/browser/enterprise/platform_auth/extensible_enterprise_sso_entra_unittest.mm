// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_entra.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "components/policy/core/common/policy_logger.h"
#import "net/base/apple/http_response_headers_util.h"
#import "net/http/http_request_headers.h"
#import "net/http/http_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// namespace enterprise_auth::util {

namespace {
constexpr NSString* kSSOCookies = @"sso_cookies";

constexpr NSString* kExampleResponse =
    @"{\"sso_cookies\":\"JSON formated "
    @"object\",\"operation\":\"get_sso_cookies\",\"broker_version\":\"3.3.23\","
    @"\"preferred_auth_config\":\"preferredAuthNotConfigured\",\"success\":"
    @"\"1\",\"wpj_status\":\"notJoined\",\"operation_response_type\":"
    @"\"operation_get_sso_cookies_response\",\"request_received_timestamp\":"
    @"\"1736954944.2245578766\",\"extraDeviceInfo\":\"{}\",\"sso_extension_"
    @"mode\":\"full\",\"device_mode\":\"personal\",\"response_gen_timestamp\":"
    @"\"1736954944.7778768539\",\"platform_sso_status\":"
    @"\"platformSSONotEnabled\"}";
constexpr char kExampleResponseHeaders1[] =
    "{\"prt_headers\":[{\"header\":{\"x-ms-RefreshTokenCredential\":"
    "\"refreshcredentialsvalue\"},\"home_account_id\":\"homeaccountid\"},{"
    "\"header\":{\"x-ms-RefreshTokenCredential1\":\"refreshcredentialsvalue1\"}"
    ",\"home_account_id\":\"homeaccountid1\"}],\"device_headers\":[{\"header\":"
    "{\"x-ms-DeviceCredential\":\"devicecredentials\"},\"tenant_id\":"
    "\"tenantidvalue\"}]}";
constexpr char kExampleResponseHeaders2[] =
    "{\"prt_headers\":[{\"header\":{},\"home_account_id\":\"homeaccountid\"},{"
    "\"header\":{\"x-ms-RefreshTokenCredential1\":\"\"}"
    ",\"home_account_id\":\"homeaccountid1\"}],\"device_headers\":[{\"tenant_"
    "id\":"
    "\"tenantidvalue\"}]}";
constexpr char kRefreshTokenCredentialName[] = "x-ms-RefreshTokenCredential";
constexpr char kRefreshTokenCredentialName1[] = "x-ms-RefreshTokenCredential1";
constexpr char kDeviceCredentialName[] = "x-ms-DeviceCredential";
constexpr char kRefreshTokenCredentialValue[] = "refreshcredentialsvalue";
constexpr char kRefreshTokenCredentialValue1[] = "refreshcredentialsvalue1";
constexpr char kDeviceCredentialValue[] = "devicecredentials";
}  // namespace

class ExtensibleEnterpriseSSOUtil : public testing::Test {
 protected:
  void SetUp() override {
    auth_provider_ = [ASAuthorizationSingleSignOnProvider
        authorizationProviderWithIdentityProviderURL:url_];
    mock_auth_provider_ = [OCMockObject partialMockForObject:auth_provider_];
    OCMStub([mock_auth_provider_ canPerformAuthorization]).andReturn(YES);
    entra_delegate_ = [[SSOServiceEntraAuthControllerDelegate alloc]
        initWithAuthorizationSingleSignOnProvider:mock_auth_provider_];
  }

  id mock_auth_provider() { return mock_auth_provider_; }
  SSOServiceEntraAuthControllerDelegate* entra_delegate() {
    return entra_delegate_;
  }
  NSURL* url() { return url_; }

 private:
  id mock_auth_provider_;
  ASAuthorizationSingleSignOnProvider* auth_provider_;
  SSOServiceEntraAuthControllerDelegate* entra_delegate_;
  NSURL* url_ = [[NSURL alloc] initWithString:@"https://www.example.com/"];
};

TEST_F(ExtensibleEnterpriseSSOUtil, CreateRequestForUrl) {
  ASAuthorizationSingleSignOnRequest* request =
      [entra_delegate() createRequest];
  NSArray* expectedOptions = @[
    [NSURLQueryItem queryItemWithName:@"sso_url" value:url().absoluteString],
    [NSURLQueryItem queryItemWithName:@"types_of_header" value:@"0"],
    [NSURLQueryItem queryItemWithName:@"msg_protocol_ver" value:@"4"],
  ];

  ASSERT_TRUE(request);
  EXPECT_EQ(request.requestedOperation, @"get_sso_cookies");
  EXPECT_TRUE([request.authorizationOptions isEqual:expectedOptions]);
  if (@available(macOS 12, *)) {
    EXPECT_EQ(request.userInterfaceEnabled, NO);
  }
}

TEST_F(ExtensibleEnterpriseSSOUtil,
       GetHeadersFromHttpResponseNoCrashWithNilResponse) {
  auto auth_headers = [entra_delegate() getHeadersFromHttpResponse:nil];
  ASSERT_TRUE(auth_headers.IsEmpty());
}

TEST_F(ExtensibleEnterpriseSSOUtil,
       GetHeadersFromHttpResponseNoCrashWithMissingHeaders) {
  // Setup response
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc] initWithURL:url()
                                                            statusCode:404
                                                           HTTPVersion:@""
                                                          headerFields:nil];

  auto auth_headers = [entra_delegate() getHeadersFromHttpResponse:response];
  ASSERT_TRUE(auth_headers.IsEmpty());
}

TEST_F(ExtensibleEnterpriseSSOUtil,
       GetHeadersFromHttpResponseNoCrashWithMissingSSOCookieJSON) {
  // Setup response
  NSMutableDictionary* allHeaderFields = [NSJSONSerialization
      JSONObjectWithData:[kExampleResponse
                             dataUsingEncoding:NSUTF8StringEncoding]
                 options:NSJSONReadingMutableContainers
                   error:nil];
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url()
                                  statusCode:404
                                 HTTPVersion:@""
                                headerFields:allHeaderFields];

  auto auth_headers = [entra_delegate() getHeadersFromHttpResponse:response];
  ASSERT_TRUE(auth_headers.IsEmpty());
}

TEST_F(ExtensibleEnterpriseSSOUtil,
       GetHeadersFromHttpResponseNoCrashWithInvalidSSOCookie) {
  // Setup response
  NSMutableDictionary* allHeaderFields = [NSJSONSerialization
      JSONObjectWithData:[kExampleResponse
                             dataUsingEncoding:NSUTF8StringEncoding]
                 options:NSJSONReadingMutableContainers
                   error:nil];
  [allHeaderFields setObject:@"{\"Invalid: json}" forKey:kSSOCookies];
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url()
                                  statusCode:404
                                 HTTPVersion:@""
                                headerFields:allHeaderFields];

  auto auth_headers = [entra_delegate() getHeadersFromHttpResponse:response];
  ASSERT_TRUE(auth_headers.IsEmpty());
}

TEST_F(ExtensibleEnterpriseSSOUtil,
       GetHeadersFromHttpResponseNoCrashWithValidEmptySSOCookie) {
  // Setup response
  NSMutableDictionary* allHeaderFields = [NSJSONSerialization
      JSONObjectWithData:[kExampleResponse
                             dataUsingEncoding:NSUTF8StringEncoding]
                 options:NSJSONReadingMutableContainers
                   error:nil];
  [allHeaderFields setObject:@"{}" forKey:kSSOCookies];
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url()
                                  statusCode:404
                                 HTTPVersion:@""
                                headerFields:allHeaderFields];

  auto auth_headers = [entra_delegate() getHeadersFromHttpResponse:response];
  ASSERT_TRUE(auth_headers.IsEmpty());
}

TEST_F(ExtensibleEnterpriseSSOUtil,
       GetHeadersFromHttpResponseNoCrashWithMissingInfoInSSOCookie) {
  // Setup response
  NSMutableDictionary* allHeaderFields = [NSJSONSerialization
      JSONObjectWithData:[kExampleResponse
                             dataUsingEncoding:NSUTF8StringEncoding]
                 options:NSJSONReadingMutableContainers
                   error:nil];
  NSString* headers_json = base::SysUTF8ToNSString(kExampleResponseHeaders2);
  [allHeaderFields setObject:headers_json forKey:kSSOCookies];
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url()
                                  statusCode:404
                                 HTTPVersion:@""
                                headerFields:allHeaderFields];

  auto auth_headers = [entra_delegate() getHeadersFromHttpResponse:response];
  ASSERT_TRUE(auth_headers.IsEmpty());
}

TEST_F(ExtensibleEnterpriseSSOUtil, GetHeadersFromHttpResponse) {
  // Setup response
  NSMutableDictionary* allHeaderFields = [NSJSONSerialization
      JSONObjectWithData:[kExampleResponse
                             dataUsingEncoding:NSUTF8StringEncoding]
                 options:NSJSONReadingMutableContainers
                   error:nil];
  NSString* headers_json = base::SysUTF8ToNSString(kExampleResponseHeaders1);
  [allHeaderFields setObject:headers_json forKey:kSSOCookies];
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url()
                                  statusCode:404
                                 HTTPVersion:@""
                                headerFields:allHeaderFields];

  auto auth_headers = [entra_delegate() getHeadersFromHttpResponse:response];
  {
    auto refresh_token = auth_headers.GetHeader(kRefreshTokenCredentialName);
    ASSERT_TRUE(refresh_token);
    EXPECT_EQ(*refresh_token, std::string(kRefreshTokenCredentialValue));
  }
  {
    auto refresh_token = auth_headers.GetHeader(kRefreshTokenCredentialName1);
    ASSERT_TRUE(refresh_token);
    EXPECT_EQ(*refresh_token, std::string(kRefreshTokenCredentialValue1));
  }
  {
    auto refresh_token = auth_headers.GetHeader(kDeviceCredentialName);
    ASSERT_TRUE(refresh_token);
    EXPECT_EQ(*refresh_token, std::string(kDeviceCredentialValue));
  }
}
