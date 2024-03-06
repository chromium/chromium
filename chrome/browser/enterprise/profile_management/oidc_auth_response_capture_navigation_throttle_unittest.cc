// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/oidc_auth_response_capture_navigation_throttle.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/mock_oidc_authentication_signin_interceptor.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationThrottle;
using testing::_;

namespace {

constexpr char kTokenTemplate[] = R"(%s.%s.%s)";
constexpr char kOidcAuthTokenFieldTemplate[] = R"(access_token=%s&)";
constexpr char kOidcIdTokenFieldTemplate[] = R"(&id_token=%s&)";
constexpr char kOidcAuthResponseTemplate[] =
    "https://chromeprofiletoken/"
    "#%stoken_type=Bearer&expires_in=5000&scope=email+openid+profile%ssession_"
    "state=abc-123";
constexpr char kDummyHeader[] = "encoded_header";
constexpr char kDummySignature[] = "signature";

constexpr char kUserPrincipleNameClaimName[] = "upn";
constexpr char kSubjectClaimName[] = "sub";

constexpr char kExampleUserPrincipleName[] = "example@org.com";
constexpr char kExampleAuthSubject[] = "example_auth_subject";
constexpr char kExampleIdSubject[] = "example_id_subject";

std::string BuildTokenFromDict(const base::Value::Dict& dict) {
  return base::StringPrintf(
      kTokenTemplate, kDummyHeader,
      base::Base64Encode(base::WriteJson(dict).value()).c_str(),
      kDummySignature);
}

std::string BuildOidcResponseUrl(const std::string& oidc_auth_token,
                                 const std::string& oidc_id_token) {
  std::string auth_token_field =
      oidc_auth_token.empty() ? std::string()
                              : base::StringPrintf(kOidcAuthTokenFieldTemplate,
                                                   oidc_auth_token.c_str());
  std::string id_token_field =
      oidc_id_token.empty() ? std::string()
                            : base::StringPrintf(kOidcIdTokenFieldTemplate,
                                                 oidc_id_token.c_str());
  return base::StringPrintf(kOidcAuthResponseTemplate, auth_token_field.c_str(),
                            id_token_field.c_str());
}

}  // namespace

namespace profile_management {

class OidcAuthResponseCaptureNavigationThrottleTest
    : public BrowserWithTestWindowTest,
      public testing::WithParamInterface<bool> {
 public:
  OidcAuthResponseCaptureNavigationThrottleTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kOidcAuthProfileManagement, enable_oidc_interception());
  }

  ~OidcAuthResponseCaptureNavigationThrottleTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    OidcAuthenticationSigninInterceptorFactory::GetInstance()
        ->SetTestingFactory(
            profile(),
            base::BindRepeating(
                [](Profile* profile, content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      MockOidcAuthenticationSigninInterceptor>(
                      profile,
                      std::make_unique<DiceWebSigninInterceptorDelegate>());
                },
                profile()));

    // Create the first tab so that web_contents() exists.
    AddTab(browser(), GURL(url::kAboutBlankURL));
  }

  void TearDown() override { BrowserWithTestWindowTest::TearDown(); }

  MockOidcAuthenticationSigninInterceptor* GetMockOidcInterceptor() {
    return static_cast<MockOidcAuthenticationSigninInterceptor*>(
        OidcAuthenticationSigninInterceptorFactory::GetForProfile(profile()));
  }

  void ExpectNoOidcInterception(
      MockOidcAuthenticationSigninInterceptor* oidc_interceptor,
      const std::string& redirection_url,
      NavigationThrottle::ThrottleAction expected_throttle_action) {
    content::MockNavigationHandle navigation_handle(GURL(redirection_url),
                                                    main_frame());
    if (!enable_oidc_interception()) {
      ASSERT_EQ(nullptr, oidc_interceptor);
    } else {
      EXPECT_CALL(*oidc_interceptor, MaybeInterceptOidcAuthentication(_, _, _))
          .Times(0);
    }
    auto throttle =
        OidcAuthResponseCaptureNavigationThrottle::MaybeCreateThrottleFor(
            &navigation_handle);

    if (!enable_oidc_interception()) {
      ASSERT_EQ(nullptr, throttle.get());
    } else {
      EXPECT_EQ(expected_throttle_action,
                throttle->WillRedirectRequest().action());
    }
    loop_.RunUntilIdle();
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  bool enable_oidc_interception() { return GetParam(); }

 protected:
  base::RunLoop loop_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, SuccessfulInterception) {
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject));

  std::string redirection_url = BuildOidcResponseUrl(auth_token, id_token);

  content::MockNavigationHandle navigation_handle(GURL(redirection_url),
                                                  main_frame());

  auto* oidc_interceptor = GetMockOidcInterceptor();
  if (!enable_oidc_interception()) {
    ASSERT_EQ(nullptr, oidc_interceptor);
  } else {
    EXPECT_CALL(*oidc_interceptor,
                MaybeInterceptOidcAuthentication(
                    web_contents(),
                    ProfileManagementOicdTokens{.auth_token = auth_token,
                                                .id_token = id_token},
                    kExampleUserPrincipleName))
        .WillOnce(testing::Return());
  }
  auto throttle =
      OidcAuthResponseCaptureNavigationThrottle::MaybeCreateThrottleFor(
          &navigation_handle);
  if (!enable_oidc_interception()) {
    ASSERT_EQ(nullptr, throttle.get());
  } else {
    EXPECT_EQ(content::NavigationThrottle::DEFER,
              throttle->WillRedirectRequest().action());
  }
  loop_.RunUntilIdle();
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, MissingAuthToken) {
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject));

  std::string redirection_url = BuildOidcResponseUrl(std::string(), id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, MissingIdToken) {
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));

  std::string redirection_url = BuildOidcResponseUrl(auth_token, std::string());

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, MissingAuthUpnClaim) {
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict().Set(kSubjectClaimName, kExampleAuthSubject));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict().Set(kSubjectClaimName, kExampleIdSubject));

  std::string redirection_url = BuildOidcResponseUrl(auth_token, id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::DEFER);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, EmptyAuthJson) {
  std::string auth_token = BuildTokenFromDict(base::Value::Dict());
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict().Set(kSubjectClaimName, kExampleIdSubject));

  std::string redirection_url = BuildOidcResponseUrl(auth_token, id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::DEFER);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
       WrongNumberOfJwtSections) {
  // Missing header and signature, this token is malformed and only has one
  // section
  std::string malformed_auth_token = base::Base64Encode(
      base::WriteJson(
          base::Value::Dict()
              .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
              .Set(kSubjectClaimName, kExampleAuthSubject))
          .value());

  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject));

  std::string redirection_url =
      BuildOidcResponseUrl(malformed_auth_token, id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, DecodeFailure) {
  // The payload section of this token is not encoded.
  std::string malformed_auth_token = base::StringPrintf(
      kTokenTemplate, kDummyHeader,
      base::WriteJson(
          base::Value::Dict()
              .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
              .Set(kSubjectClaimName, kExampleAuthSubject))
          .value()
          .c_str(),
      kDummySignature);

  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject));

  std::string redirection_url =
      BuildOidcResponseUrl(malformed_auth_token, id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, DataDecoderFailure) {
  in_process_data_decoder_.SimulateJsonParserCrash(/*drop=*/true);
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject));

  std::string redirection_url = BuildOidcResponseUrl(auth_token, id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::DEFER);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OidcAuthResponseCaptureNavigationThrottleTest,
                         /*enable_oidc_interception=*/testing::Bool());

}  // namespace profile_management
