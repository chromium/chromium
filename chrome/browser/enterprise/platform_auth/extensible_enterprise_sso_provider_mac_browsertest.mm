// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#import "base/run_loop.h"
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
class ExtensibleEnterpriseSSOTest : public InProcessBrowserTest {
 public:
  GURL url() { return GURL(kUrl); }
  NSURL* nativeUrl() { return net::NSURLWithGURL(url()); }
};

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSSOTest, DISABLED_SupportedFail) {
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
  OCMStub([mock_provider_class createRequest]).andReturn(request);

  // Create mock delegate to verify that the failure function is called.
  SSOServiceEntraAuthControllerDelegate* delegate =
      [[SSOServiceEntraAuthControllerDelegate alloc]
          initWithAuthorizationSingleSignOnProvider:mock_provider_class];
  id delegate_mock = [OCMockObject partialMockForObject:delegate];
  OCMExpect([delegate_mock authorizationController:[OCMArg isNotNil]
                              didCompleteWithError:[OCMArg isNotNil]])
      .andForwardToRealObject();

  // Ensure creating a `SSOServiceEntraAuthControllerDelegate` object returns
  // `delegate_mock`.
  id mock_delegate_class =
      OCMClassMock([SSOServiceEntraAuthControllerDelegate class]);
  OCMStub([mock_delegate_class alloc]).andReturn(mock_delegate_class);
  OCMStub([mock_delegate_class
              initWithAuthorizationSingleSignOnProvider:[OCMArg any]])
      .andReturn(delegate_mock);

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
  }
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSSOTest, Unsupported) {
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
  }
}

}  // namespace enterprise_auth
