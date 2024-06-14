// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/oidc_auth_response_capture_navigation_throttle.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/mock_oidc_authentication_signin_interceptor.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"
#include "chrome/browser/enterprise/signin/oidc_metrics_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
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
    "https://chromeenterprise.google/enroll/"
    "#%stoken_type=Bearer&expires_in=5000&scope=email+openid+profile%ssession_"
    "state=abc-123";
constexpr char kDummyHeader[] = "encoded_header";
constexpr char kDummySignature[] = "signature";

constexpr char kOidcEntraReprocessUrl[] =
    "https://login.microsoftonline.com/common/"
    "reprocess?some_encoded_value&session_id=123";

constexpr char kOidcEntraKmsiUrl[] = "https://login.microsoftonline.com/kmsi";

constexpr char kUserPrincipleNameClaimName[] = "upn";
constexpr char kSubjectClaimName[] = "sub";
constexpr char kIssuerClaimName[] = "iss";

constexpr char kExampleUserPrincipleName[] = "example@org.com";
constexpr char kExampleAuthSubject[] = "example_auth_subject";
constexpr char kExampleIdSubject[] = "example_id_subject";
constexpr char kExampleIdIssuer[] = "example_id_issuer";

const char kOidcEnrollmentHistogramName[] = "Enterprise.OidcEnrollment";

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
    content::MockNavigationHandle navigation_handle(
        GURL(kOidcEntraReprocessUrl), main_frame());
    if (!enable_oidc_interception()) {
      ASSERT_EQ(nullptr, oidc_interceptor);
    } else {
      EXPECT_CALL(*oidc_interceptor,
                  MaybeInterceptOidcAuthentication(_, _, _, _, _))
          .Times(0);
    }
    auto throttle =
        OidcAuthResponseCaptureNavigationThrottle::MaybeCreateThrottleFor(
            &navigation_handle);

    if (!enable_oidc_interception()) {
      ASSERT_EQ(nullptr, throttle.get());
    } else {
      if (expected_throttle_action == NavigationThrottle::DEFER) {
        throttle->set_resume_callback_for_testing(
            task_environment()->QuitClosure());
      }
      navigation_handle.set_url(GURL(redirection_url));
      EXPECT_EQ(expected_throttle_action,
                throttle->WillProcessResponse().action());

      if (expected_throttle_action == NavigationThrottle::DEFER) {
        task_environment()->RunUntilQuit();
      } else {
        task_environment()->RunUntilIdle();
      }
    }
  }

  void TestNoServiceForInvalidProfile(Profile* invalid_profile) {
    std::string auth_token = BuildTokenFromDict(
        base::Value::Dict()
            .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
            .Set(kSubjectClaimName, kExampleAuthSubject));
    std::string id_token = BuildTokenFromDict(
        base::Value::Dict()
            .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
            .Set(kSubjectClaimName, kExampleIdSubject)
            .Set(kIssuerClaimName, kExampleIdIssuer));

    auto* oidc_interceptor =
        static_cast<MockOidcAuthenticationSigninInterceptor*>(
            OidcAuthenticationSigninInterceptorFactory::GetForProfile(
                invalid_profile));
    auto test_web_content = content::WebContents::Create(
        content::WebContents::CreateParams(invalid_profile));
    content::MockNavigationHandle navigation_handle(test_web_content.get());

    navigation_handle.set_url(GURL(kOidcEntraReprocessUrl));
    ASSERT_EQ(nullptr, oidc_interceptor);

    auto throttle =
        OidcAuthResponseCaptureNavigationThrottle::MaybeCreateThrottleFor(
            &navigation_handle);

    if (!enable_oidc_interception()) {
      ASSERT_EQ(nullptr, throttle.get());
    } else {
      navigation_handle.set_url(
          GURL(BuildOidcResponseUrl(auth_token, id_token)));
      EXPECT_EQ(NavigationThrottle::PROCEED,
                throttle->WillProcessResponse().action());
      task_environment()->RunUntilIdle();
      CheckFunnelAndResultHistogram(
          OidcInterceptionFunnelStep::kValidRedirectionCaptured,
          OidcInterceptionResult::kInvalidProfile);
    }
  }

  void CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep expected_last_funnel_step,
      std::optional<OidcInterceptionResult> expected_enrollment_result) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({kOidcEnrollmentHistogramName, ".Interception.Funnel"}),
        expected_last_funnel_step, enable_oidc_interception() ? 1 : 0);

    if (expected_enrollment_result == std::nullopt) {
      return;
    }

    histogram_tester_.ExpectUniqueSample(
        base::StrCat({kOidcEnrollmentHistogramName, ".Interception.Result"}),
        expected_enrollment_result.value(), enable_oidc_interception() ? 1 : 0);
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  bool enable_oidc_interception() { return GetParam(); }

 protected:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
       DirectNavigationDoesNoTrigger) {
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject)
          .Set(kIssuerClaimName, kExampleIdIssuer));

  std::string direct_navigate_url = BuildOidcResponseUrl(auth_token, id_token);

  content::MockNavigationHandle navigation_handle(GURL(direct_navigate_url),
                                                  main_frame());
  auto* oidc_interceptor = GetMockOidcInterceptor();

  if (!enable_oidc_interception()) {
    ASSERT_EQ(nullptr, oidc_interceptor);
  } else {
    EXPECT_CALL(*oidc_interceptor,
                MaybeInterceptOidcAuthentication(_, _, _, _, _))
        .Times(0);
  }

  auto throttle =
      OidcAuthResponseCaptureNavigationThrottle::MaybeCreateThrottleFor(
          &navigation_handle);

  ASSERT_EQ(nullptr, throttle.get());
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, SuccessfulInterception) {
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject)
          .Set(kIssuerClaimName, kExampleIdIssuer));

  std::string redirection_url = BuildOidcResponseUrl(auth_token, id_token);

  content::MockNavigationHandle navigation_handle(GURL(kOidcEntraReprocessUrl),
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
                    kExampleIdIssuer, kExampleIdSubject, _))
        .WillOnce([](content::WebContents* intercepted_contents,
                     ProfileManagementOicdTokens oidc_tokens,
                     std::string issuer_id, std::string subject_id,
                     OidcInterceptionCallback oidc_callback) {
          std::move(oidc_callback).Run();
        });
  }

  auto throttle =
      OidcAuthResponseCaptureNavigationThrottle::MaybeCreateThrottleFor(
          &navigation_handle);
  if (!enable_oidc_interception()) {
    ASSERT_EQ(nullptr, throttle.get());
  } else {
    throttle->set_resume_callback_for_testing(
        task_environment()->QuitClosure());
    navigation_handle.set_url(GURL(redirection_url));
    EXPECT_EQ(NavigationThrottle::DEFER,
              throttle->WillProcessResponse().action());
    task_environment()->RunUntilQuit();
  }

  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kSuccessfulInfoParsed, std::nullopt);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, MissingAuthToken) {
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject)
          .Set(kIssuerClaimName, kExampleIdIssuer));

  std::string redirection_url = BuildOidcResponseUrl(std::string(), id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::PROCEED);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, MissingIdToken) {
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));

  std::string redirection_url = BuildOidcResponseUrl(auth_token, std::string());

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::PROCEED);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, MsftKmsiThrottling) {
  content::MockNavigationHandle navigation_handle(GURL(kOidcEntraKmsiUrl),
                                                  main_frame());
  auto throttle =
      OidcAuthResponseCaptureNavigationThrottle::MaybeCreateThrottleFor(
          &navigation_handle);

  if (!enable_oidc_interception()) {
    ASSERT_EQ(nullptr, throttle.get());
  } else {
    ASSERT_NE(nullptr, throttle.get());
  }
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, MissingIdTokenSubClaim) {
  std::string auth_token = BuildTokenFromDict(base::Value::Dict().Set(
      kUserPrincipleNameClaimName, kExampleUserPrincipleName));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kIssuerClaimName, kExampleIdIssuer));

  std::string redirection_url = BuildOidcResponseUrl(auth_token, id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::DEFER);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, MissingIdTokenIssClaim) {
  std::string auth_token = BuildTokenFromDict(base::Value::Dict().Set(
      kUserPrincipleNameClaimName, kExampleUserPrincipleName));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject));

  std::string redirection_url = BuildOidcResponseUrl(auth_token, id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::DEFER);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, EmptyIdJson) {
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict().Set(kSubjectClaimName, kExampleAuthSubject));
  std::string id_token = BuildTokenFromDict(base::Value::Dict());

  std::string redirection_url = BuildOidcResponseUrl(auth_token, id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::DEFER);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
       WrongNumberOfJwtSections) {
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));

  // Missing header and signature, this token is malformed and only has one
  // section
  std::string malformed_id_token = base::Base64Encode(
      base::WriteJson(
          base::Value::Dict()
              .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
              .Set(kSubjectClaimName, kExampleIdSubject))
          .value());

  std::string redirection_url =
      BuildOidcResponseUrl(auth_token, malformed_id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::CANCEL_AND_IGNORE);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, DecodeFailure) {
  // The payload section of this token is not encoded.
  std::string malformed_id_token = base::StringPrintf(
      kTokenTemplate, kDummyHeader,
      base::WriteJson(
          base::Value::Dict()
              .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
              .Set(kSubjectClaimName, kExampleAuthSubject))
          .value()
          .c_str(),
      kDummySignature);

  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));

  std::string redirection_url =
      BuildOidcResponseUrl(auth_token, malformed_id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::CANCEL_AND_IGNORE);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
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
          .Set(kSubjectClaimName, kExampleIdSubject)
          .Set(kIssuerClaimName, kExampleIdIssuer));

  std::string redirection_url = BuildOidcResponseUrl(auth_token, id_token);

  auto* oidc_interceptor = GetMockOidcInterceptor();
  ExpectNoOidcInterception(oidc_interceptor, redirection_url,
                           NavigationThrottle::DEFER);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, NoServiceForGuestMode) {
  TestNoServiceForInvalidProfile(profile_manager()->CreateGuestProfile());
}

TEST_P(OidcAuthResponseCaptureNavigationThrottleTest, NoServiceForIncognito) {
  TestNoServiceForInvalidProfile(profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true));
}

INSTANTIATE_TEST_SUITE_P(All,
                         OidcAuthResponseCaptureNavigationThrottleTest,
                         /*enable_oidc_interception=*/testing::Bool());

}  // namespace profile_management
