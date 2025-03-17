// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/mock_callback.h"
#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_entra.h"
#import "chrome/test/base/in_process_browser_test.h"
#import "content/public/test/browser_test.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

using ::testing::_;

constexpr char kUrl[] = "https://www.example.com/";

namespace enterprise_auth {

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
constexpr char kRefreshTokenCredentialName[] = "x-ms-RefreshTokenCredential";
constexpr char kRefreshTokenCredentialName1[] = "x-ms-RefreshTokenCredential1";
constexpr char kDeviceCredentialName[] = "x-ms-DeviceCredential";
constexpr char kRefreshTokenCredentialValue[] = "refreshcredentialsvalue";
constexpr char kRefreshTokenCredentialValue1[] = "refreshcredentialsvalue1";
constexpr char kDeviceCredentialValue[] = "devicecredentials";

}  // namespace

class ExtensibleEnterpriseSSOTest : public InProcessBrowserTest {
 public:
  GURL url() { return GURL(kUrl); }
  NSURL* nativeUrl() { return net::NSURLWithGURL(url()); }
};

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSSOTest, DISABLED_SupportedFail) {
  base::HistogramTester histogram_tester;

  // Create a fake request
  ASAuthorizationSingleSignOnRequest* request =
      [[ASAuthorizationSingleSignOnProvider
          authorizationProviderWithIdentityProviderURL:nativeUrl()]
          createRequest];

  // Mock the provider to ensure supported authorization
  id mock_provider_class =
      OCMClassMock([ASAuthorizationSingleSignOnProvider class]);
  OCMStub([mock_provider_class alloc]).andReturn(mock_provider_class);
  OCMStub([mock_provider_class
              authorizationProviderWithIdentityProviderURL:[OCMArg any]])
      .andReturn(mock_provider_class);
  OCMStub([mock_provider_class canPerformAuthorization]).andReturn(YES);

  // Create mock delegate to verify that the failure function is called.
  SSOServiceEntraAuthControllerDelegate* delegate =
      [[SSOServiceEntraAuthControllerDelegate alloc]
          initWithAuthorizationSingleSignOnProvider:mock_provider_class];
  id delegate_mock = [OCMockObject partialMockForObject:delegate];
  OCMExpect([delegate_mock authorizationController:[OCMArg any]
                              didCompleteWithError:[OCMArg any]])
      .andForwardToRealObject();

  // Ensure creating a `SSOServiceEntraAuthControllerDelegate` object returns
  // `delegate_mock`.
  id mock_delegate_class =
      OCMClassMock([SSOServiceEntraAuthControllerDelegate class]);
  OCMStub([mock_delegate_class alloc]).andReturn(mock_delegate_class);
  OCMStub([mock_delegate_class
              initWithAuthorizationSingleSignOnProvider:[OCMArg any]])
      .andReturn(delegate_mock);

  OCMStub([delegate_mock performRequest]).andDo(^(NSInvocation*) {
    [delegate_mock
        authorizationController:[[ASAuthorizationController alloc]
                                    initWithAuthorizationRequests:@[ request ]]
           didCompleteWithError:[NSError errorWithDomain:@"domain"
                                                    code:0
                                                userInfo:@{}]];
  });

  ExtensibleEnterpriseSSOProvider provider;
  {
    base::RunLoop run_loop;
    net::HttpRequestHeaders response_headers;
    base::MockCallback<PlatformAuthProviderManager::GetDataCallback> mock;
    EXPECT_CALL(mock, Run(_))
        .WillOnce(
            [&run_loop, &response_headers](net::HttpRequestHeaders headers) {
              response_headers = headers;
              run_loop.Quit();
            });

    provider.GetData(url(), mock.Get());
    run_loop.Run();

    // On failure no headers are returned.
    EXPECT_TRUE(response_headers.IsEmpty());

    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.NotSupported.Duration", 0);
    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Success.Duration", 0);
    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Failure.Duration", 1);
    histogram_tester.ExpectBucketCount(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Result", false, 1);
    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Result", 1);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSSOTest, DISABLED_SupportedSuccess) {
  base::HistogramTester histogram_tester;

  // Create a fake request
  ASAuthorizationSingleSignOnRequest* request =
      [[ASAuthorizationSingleSignOnProvider
          authorizationProviderWithIdentityProviderURL:nativeUrl()]
          createRequest];

  // Mock the provider to ensure supported authorization
  id mock_provider_class =
      OCMClassMock([ASAuthorizationSingleSignOnProvider class]);
  OCMStub([mock_provider_class alloc]).andReturn(mock_provider_class);
  OCMStub([mock_provider_class
              authorizationProviderWithIdentityProviderURL:[OCMArg any]])
      .andReturn(mock_provider_class);
  OCMStub([mock_provider_class canPerformAuthorization]).andReturn(YES);

  // Create mock delegate to verify that the failure function is called.
  SSOServiceEntraAuthControllerDelegate* delegate =
      [[SSOServiceEntraAuthControllerDelegate alloc]
          initWithAuthorizationSingleSignOnProvider:mock_provider_class];
  id delegate_mock = [OCMockObject partialMockForObject:delegate];
  OCMExpect([delegate_mock authorizationController:[OCMArg any]
                      didCompleteWithAuthorization:[OCMArg any]])
      .andForwardToRealObject();

  // Ensure creating a `SSOServiceEntraAuthControllerDelegate` object returns
  // `delegate_mock`.
  id mock_delegate_class =
      OCMClassMock([SSOServiceEntraAuthControllerDelegate class]);
  OCMStub([mock_delegate_class alloc]).andReturn(mock_delegate_class);
  OCMStub([mock_delegate_class
              initWithAuthorizationSingleSignOnProvider:[OCMArg any]])
      .andReturn(delegate_mock);

  // Setup response
  NSMutableDictionary* allHeaderFields = [NSJSONSerialization
      JSONObjectWithData:[kExampleResponse
                             dataUsingEncoding:NSUTF8StringEncoding]
                 options:NSJSONReadingMutableContainers
                   error:nil];
  NSString* headers_json = base::SysUTF8ToNSString(kExampleResponseHeaders1);
  [allHeaderFields setObject:headers_json forKey:kSSOCookies];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[[NSURL alloc] initWithString:@"https://www.example.com/"]
        statusCode:404
       HTTPVersion:@""
      headerFields:allHeaderFields];

  // Mock an authorization.
  id mock_credential_class =
      OCMClassMock([ASAuthorizationSingleSignOnCredential class]);
  OCMStub([mock_credential_class alloc]).andReturn(mock_credential_class);
  OCMStub([mock_credential_class authenticatedResponse]).andReturn(response);

  id mock_authorization_class = OCMClassMock([ASAuthorization class]);
  OCMStub([mock_authorization_class alloc]).andReturn(mock_authorization_class);
  OCMStub([mock_authorization_class credential])
      .andReturn(mock_credential_class);

  OCMStub([delegate_mock performRequest]).andDo(^(NSInvocation*) {
    [delegate_mock authorizationController:[[ASAuthorizationController alloc]
                                               initWithAuthorizationRequests:@[
                                                 request
                                               ]]
              didCompleteWithAuthorization:mock_authorization_class];
  });

  ExtensibleEnterpriseSSOProvider provider;
  {
    base::RunLoop run_loop;
    net::HttpRequestHeaders response_headers;
    base::MockCallback<PlatformAuthProviderManager::GetDataCallback> mock;
    EXPECT_CALL(mock, Run(_))
        .WillOnce(
            [&run_loop, &response_headers](net::HttpRequestHeaders headers) {
              response_headers = headers;
              run_loop.Quit();
            });

    provider.GetData(url(), mock.Get());
    run_loop.Run();

    // On success the right headers are returned.
    EXPECT_FALSE(response_headers.IsEmpty());
    {
      auto refresh_token =
          response_headers.GetHeader(kRefreshTokenCredentialName);
      ASSERT_TRUE(refresh_token);
      EXPECT_EQ(*refresh_token, std::string(kRefreshTokenCredentialValue));
    }
    {
      auto refresh_token =
          response_headers.GetHeader(kRefreshTokenCredentialName1);
      ASSERT_TRUE(refresh_token);
      EXPECT_EQ(*refresh_token, std::string(kRefreshTokenCredentialValue1));
    }
    {
      auto refresh_token = response_headers.GetHeader(kDeviceCredentialName);
      ASSERT_TRUE(refresh_token);
      EXPECT_EQ(*refresh_token, std::string(kDeviceCredentialValue));
    }

    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.NotSupported.Duration", 0);
    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Success.Duration", 1);
    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Failure.Duration", 0);
    histogram_tester.ExpectBucketCount(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Result", true, 1);
    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Result", 1);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSSOTest, Unsupported) {
  base::HistogramTester histogram_tester;

  // Mock the provider to authorization not supported.
  id mock_provider_class =
      OCMClassMock([ASAuthorizationSingleSignOnProvider class]);
  OCMStub([mock_provider_class alloc]).andReturn(mock_provider_class);
  OCMStub([mock_provider_class
              authorizationProviderWithIdentityProviderURL:[OCMArg any]])
      .andReturn(mock_provider_class);
  OCMStub([mock_provider_class canPerformAuthorization]).andReturn(NO);

  // Ensure `SSOServiceEntraAuthControllerDelegate` never created.
  id mock_delegate_class =
      OCMStrictClassMock([SSOServiceEntraAuthControllerDelegate class]);
  OCMStub([mock_delegate_class alloc]).andReturn(mock_delegate_class);

  ExtensibleEnterpriseSSOProvider provider;
  {
    base::RunLoop run_loop;
    net::HttpRequestHeaders response_headers;
    base::MockCallback<PlatformAuthProviderManager::GetDataCallback> mock;
    EXPECT_CALL(mock, Run(_))
        .WillOnce(
            [&run_loop, &response_headers](net::HttpRequestHeaders headers) {
              response_headers = headers;
              run_loop.Quit();
            });

    provider.GetData(url(), mock.Get());
    run_loop.Run();
    // On failure no headers are returned.
    EXPECT_TRUE(response_headers.IsEmpty());

    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.NotSupported.Duration", 1);
    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Success.Duration", 0);
    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Failure.Duration", 0);
    histogram_tester.ExpectTotalCount(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Result", 0);
  }
}

}  // namespace enterprise_auth
