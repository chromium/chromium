// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/oidc_auth_response_capture_navigation_throttle.h"

#include <memory>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/mock_oidc_authentication_signin_interceptor.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"
#include "chrome/browser/enterprise/signin/oidc_metrics_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/test_renderer_host.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::FeatureRef;
using base::test::FeatureRefAndParams;
using content::NavigationThrottle;
using testing::_;

namespace {

constexpr char kTokenTemplate[] = R"(%s.%s.%s)";
constexpr char kOidcAuthTokenFieldTemplate[] = R"(access_token=%s&)";
constexpr char kOidcIdTokenFieldTemplate[] = R"(&id_token=%s&)";
constexpr char kOidcStateFieldTemplate[] = R"(&state=%s&)";
constexpr char kOidcAuthResponseTemplate[] =
    "https://chromeenterprise.google/enroll/"
    "#%stoken_type=Bearer&expires_in=5000&scope=email+openid+profile%s%"
    "ssession_"
    "state=abc-123";
constexpr char kDummyHeader[] = "encoded_header";
constexpr char kDummySignature[] = "signature";

// constexpr char kRegistrationHeaderField[] = "X-Profile-Registration-Payload";

constexpr char kOidcEntraReprocessUrl[] =
    "https://login.microsoftonline.com/common/"
    "reprocess?some_encoded_value&session_id=123";
constexpr char kOidcNonEntraReprocessUrl[] =
    "https://test.com/common/reprocess?some_encoded_value&session_id=123";

constexpr char kOidcEntraKmsiUrl[] = "https://login.microsoftonline.com/kmsi";
constexpr char kOidcState[] = "1234";

constexpr char kHeaderInterceptionTestUrl[] =
    "https://chromeenterprise.google/profile-enrollment/register-handler";

constexpr char kUserPrincipleNameClaimName[] = "upn";
constexpr char kSubjectClaimName[] = "sub";
constexpr char kIssuerClaimName[] = "iss";

constexpr char kExampleUserPrincipleName[] = "example@org.com";
constexpr char kExampleAuthSubject[] = "example_auth_subject";
constexpr char kExampleIdSubject[] = "example_id_subject";
constexpr char kExampleIdIssuer[] = "example_id_issuer";
constexpr char kExampleEncodedInfo[] = "EncodedMessageInBase64";

constexpr char kOidcEnrollmentHistogramName[] = "Enterprise.OidcEnrollment";
constexpr char kProfileEnrollmentUkm[] = "Enterprise.Profile.Enrollment";

std::string BuildTokenFromDict(const base::Value::Dict& dict) {
  return base::StringPrintf(
      kTokenTemplate, kDummyHeader,
      base::Base64Encode(base::WriteJson(dict).value()).c_str(),
      kDummySignature);
}

std::string BuildOidcResponseUrl(const std::string& oidc_auth_token,
                                 const std::string& oidc_id_token,
                                 const std::string& oidc_state) {
  std::string auth_token_field =
      oidc_auth_token.empty() ? std::string()
                              : base::StringPrintf(kOidcAuthTokenFieldTemplate,
                                                   oidc_auth_token.c_str());
  std::string id_token_field =
      oidc_id_token.empty() ? std::string()
                            : base::StringPrintf(kOidcIdTokenFieldTemplate,
                                                 oidc_id_token.c_str());
  std::string state_field =
      oidc_state.empty()
          ? std::string()
          : base::StringPrintf(kOidcStateFieldTemplate, oidc_state.c_str());

  return base::StringPrintf(kOidcAuthResponseTemplate, auth_token_field.c_str(),
                            id_token_field.c_str(), state_field.c_str());
}

// Convenient helper function that builds valid OIDC response URL using valid
// tokens
std::string BuildStandardResponseUrl(const std::string& oidc_state) {
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject)
          .Set(kIssuerClaimName, kExampleIdIssuer));
  return BuildOidcResponseUrl(auth_token, id_token, oidc_state);
}

scoped_refptr<net::HttpResponseHeaders> BuildExampleResponseHeader(
    std::string issuer = kExampleIdIssuer,
    std::string subject_id = kExampleIdSubject,
    std::string encoded_user_info = kExampleEncodedInfo) {
  enterprise_management::ProfileRegistrationPayload registration_payload;
  registration_payload.set_issuer(issuer);
  registration_payload.set_subject(subject_id);
  registration_payload.set_encrypted_user_information(encoded_user_info);
  std::string payload_string;
  std::string encoded_payload;
  registration_payload.SerializeToString(&payload_string);
  base::Base64UrlEncode(payload_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_payload);
  std::string raw_response_headers =
      "HTTP/1.1 200 OK\r\n"
      "X-Profile-Registration-Payload:" +
      encoded_payload + "\r\n";
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_response_headers));
}

// Return the child of `parent`.
content::RenderFrameHost* GetChild(content::RenderFrameHost& parent) {
  content::RenderFrameHost* child_rfh = nullptr;
  parent.ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    if (&parent == rfh->GetParent()) {
      child_rfh = rfh;
    }
  });
  return child_rfh;
}

}  // namespace

namespace profile_management {

bool IsSourceUrlValid(std::string url_string) {
  return !OidcAuthResponseCaptureNavigationThrottle::
              GetOidcEnrollmentUrlMatcherForTesting()
                  ->MatchURL(GURL(url_string))
                  .empty();
}

void ExpandFeatureList(std::vector<FeatureRefAndParams>& enabled_features,
                       std::vector<FeatureRef>& disabled_features,
                       const base::flat_map<FeatureRef, bool>& feature_states) {
  for (const auto& [feature, enabled] : feature_states) {
    if (enabled) {
      enabled_features.push_back({*feature, {{}}});
    } else {
      disabled_features.push_back(feature);
    }
  }
}

class OidcAuthResponseCaptureNavigationThrottleTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  explicit OidcAuthResponseCaptureNavigationThrottleTest(
      const std::string& additional_hosts) {
    std::vector<FeatureRefAndParams> enabled_features;
    std::vector<FeatureRef> disabled_features;

    ExpandFeatureList(
        enabled_features, disabled_features,
        {{features::kOidcAuthProfileManagement, enable_oidc_interception()},
         {features::kEnableGenericOidcAuthProfileManagement,
          enable_generic_oidc()},
         {features::kOidcAuthResponseInterception, enable_process_response()}});

    if (!additional_hosts.empty()) {
      enabled_features.push_back(
          {features::kOidcEnrollmentAuthSource,
           {{features::kOidcAuthAdditionalHosts.name, additional_hosts}}});
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  OidcAuthResponseCaptureNavigationThrottleTest()
      : OidcAuthResponseCaptureNavigationThrottleTest(
            /*additional_hosts=*/std::string()) {}

  ~OidcAuthResponseCaptureNavigationThrottleTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    in_process_data_decoder_ =
        std::make_unique<data_decoder::test::InProcessDataDecoder>();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    OidcAuthenticationSigninInterceptorFactory::GetInstance()
        ->SetTestingFactory(
            browser()->profile(),
            base::BindRepeating(
                [](Profile* profile, content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      MockOidcAuthenticationSigninInterceptor>(
                      profile,
                      std::make_unique<DiceWebSigninInterceptorDelegate>());
                },
                browser()->profile()));

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  MockOidcAuthenticationSigninInterceptor* GetMockOidcInterceptor() {
    return static_cast<MockOidcAuthenticationSigninInterceptor*>(
        OidcAuthenticationSigninInterceptorFactory::GetForProfile(
            browser()->profile()));
  }

  void ValidateOidcTokens(ProfileManagementOidcTokens tokens,
                          ProfileManagementOidcTokens expected_tokens) {
    EXPECT_EQ(tokens.auth_token, expected_tokens.auth_token);
    EXPECT_EQ(tokens.id_token, expected_tokens.id_token);
    EXPECT_EQ(tokens.identity_name, expected_tokens.identity_name);
    EXPECT_EQ(tokens.state, expected_tokens.state);
  }

  void SetupRedirectionForHandle(
      content::MockNavigationHandle& navigation_handle,
      std::vector<GURL> source_urls,
      const GURL& last_url) {
    navigation_handle.set_url(last_url);
    navigation_handle.set_redirect_chain(source_urls);
  }

  void RunThrottleAndExpectNoOidcInterception(
      MockOidcAuthenticationSigninInterceptor* oidc_interceptor,
      const std::string& redirection_url,
      NavigationThrottle::ThrottleAction expected_throttle_action) {
    base::RunLoop run_loop;

    content::MockNavigationHandle navigation_handle(
        GURL(kOidcEntraReprocessUrl), main_frame());

    EXPECT_CALL(*oidc_interceptor,
                MaybeInterceptOidcAuthentication(_, _, _, _, _, _))
        .Times(0);

    content::MockNavigationThrottleRegistry registry(
        &navigation_handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
    ASSERT_EQ(1u, registry.throttles().size());
    auto* throttle = registry.throttles().back().get();

    if (expected_throttle_action == NavigationThrottle::DEFER) {
      throttle->set_resume_callback_for_testing(run_loop.QuitClosure());
    }

    SetupRedirectionForHandle(
        navigation_handle,
        {GURL(kOidcEntraReprocessUrl), GURL(redirection_url)},
        GURL(redirection_url));
    EXPECT_EQ(expected_throttle_action,
              throttle->WillRedirectRequest().action());

    if (expected_throttle_action == NavigationThrottle::DEFER) {
      run_loop.Run();
    }
  }

  void ExpectOidcInterception(
      MockOidcAuthenticationSigninInterceptor* oidc_interceptor,
      ProfileManagementOidcTokens expected_oidc_tokens) {
    EXPECT_CALL(*oidc_interceptor, MaybeInterceptOidcAuthentication(
                                       web_contents(), _, kExampleIdIssuer,
                                       kExampleIdSubject, _, _))
        .WillOnce([this, expected_oidc_tokens](
                      content::WebContents* intercepted_contents,
                      ProfileManagementOidcTokens oidc_tokens,
                      std::string issuer_id, std::string subject_id,
                      std::string email,
                      OidcInterceptionCallback oidc_callback) {
          ValidateOidcTokens(oidc_tokens, expected_oidc_tokens);
          std::move(oidc_callback).Run();
        });
  }

  void TestNoServiceForInvalidProfile(Profile* invalid_profile) {
    auto* oidc_interceptor =
        static_cast<MockOidcAuthenticationSigninInterceptor*>(
            OidcAuthenticationSigninInterceptorFactory::GetForProfile(
                invalid_profile));
    auto test_web_content = content::WebContents::Create(
        content::WebContents::CreateParams(invalid_profile));
    content::MockNavigationHandle navigation_handle(test_web_content.get());

    navigation_handle.set_url(GURL(kOidcEntraReprocessUrl));
    ASSERT_EQ(nullptr, oidc_interceptor);

    content::MockNavigationThrottleRegistry registry(
        &navigation_handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
    ASSERT_EQ(1u, registry.throttles().size());

    std::string redirection_url =
        BuildStandardResponseUrl(/*oidc_state=*/std::string());
    SetupRedirectionForHandle(
        navigation_handle,
        {GURL(kOidcEntraReprocessUrl), GURL(redirection_url)},
        GURL(redirection_url));

    EXPECT_EQ(NavigationThrottle::PROCEED,
              registry.throttles().back()->WillRedirectRequest().action());

    CheckFunnelAndResultHistogram(
        OidcInterceptionFunnelStep::kValidRedirectionCaptured,
        OidcInterceptionResult::kInvalidProfile);
  }

  void TestInterceptionForUrl(bool add_oidc_state,
                              bool should_log_ukm,
                              std::string source_url) {
    base::RunLoop run_loop;
    std::string auth_token = BuildTokenFromDict(
        base::Value::Dict()
            .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
            .Set(kSubjectClaimName, kExampleAuthSubject));
    std::string id_token = BuildTokenFromDict(
        base::Value::Dict()
            .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
            .Set(kSubjectClaimName, kExampleIdSubject)
            .Set(kIssuerClaimName, kExampleIdIssuer));
    std::string oidc_state =
        (enable_generic_oidc() && add_oidc_state) ? kOidcState : std::string();

    std::string redirection_url =
        BuildOidcResponseUrl(auth_token, id_token, oidc_state);
    content::MockNavigationHandle navigation_handle(GURL(source_url),
                                                    main_frame());

    auto* oidc_interceptor = GetMockOidcInterceptor();
    if (enable_generic_oidc() || IsSourceUrlValid(source_url)) {
      ExpectOidcInterception(
          oidc_interceptor,
          ProfileManagementOidcTokens(auth_token, id_token, oidc_state));
    } else {
      EXPECT_CALL(*oidc_interceptor,
                  MaybeInterceptOidcAuthentication(_, _, _, _, _, _))
          .Times(0);
    }

    content::MockNavigationThrottleRegistry registry(
        &navigation_handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
    ASSERT_EQ(1u, registry.throttles().size());
    auto* throttle = registry.throttles().back().get();

    SetupRedirectionForHandle(navigation_handle,
                              {GURL(source_url), GURL(redirection_url)},
                              GURL(redirection_url));

    if (!enable_generic_oidc() && !IsSourceUrlValid(source_url)) {
      EXPECT_EQ(NavigationThrottle::PROCEED,
                throttle->WillRedirectRequest().action());
    } else {
      throttle->set_resume_callback_for_testing(run_loop.QuitClosure());
      EXPECT_EQ(NavigationThrottle::DEFER,
                throttle->WillRedirectRequest().action());
      run_loop.Run();
    }

    if (should_log_ukm) {
      ExpectUkmLogged(navigation_handle.GetNavigationId());
    } else {
      ExpectNoUkmLogged();
    }
  }

  void TestHeaderInterceptionForUrl(std::string source_url) {
    base::RunLoop run_loop;

    content::MockNavigationHandle navigation_handle(GURL(source_url),
                                                    main_frame());
    navigation_handle.set_response_headers(BuildExampleResponseHeader());

    auto* oidc_interceptor = GetMockOidcInterceptor();
    ExpectOidcInterception(
        oidc_interceptor,
        ProfileManagementOidcTokens(std::string(), kExampleEncodedInfo,
                                    std::string()));

    content::MockNavigationThrottleRegistry registry(
        &navigation_handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
    ASSERT_EQ(1u, registry.throttles().size());
    auto* throttle = registry.throttles().back().get();

    throttle->set_resume_callback_for_testing(run_loop.QuitClosure());
    EXPECT_EQ(NavigationThrottle::DEFER,
              throttle->WillProcessResponse().action());
    run_loop.Run();
  }

  void ExpectHeadersInvalid(scoped_refptr<net::HttpResponseHeaders> headers) {
    content::MockNavigationHandle navigation_handle(
        GURL(kHeaderInterceptionTestUrl), main_frame());
    navigation_handle.set_response_headers(headers);

    auto* oidc_interceptor = GetMockOidcInterceptor();
    EXPECT_CALL(*oidc_interceptor,
                MaybeInterceptOidcAuthentication(_, _, _, _, _, _))
        .Times(0);

    content::MockNavigationThrottleRegistry registry(
        &navigation_handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
    ASSERT_EQ(1u, registry.throttles().size());
    auto* throttle = registry.throttles().back().get();

    EXPECT_EQ(NavigationThrottle::PROCEED,
              throttle->WillProcessResponse().action());
    CheckFunnelAndResultHistogram(
        OidcInterceptionFunnelStep::kValidRedirectionCaptured,
        OidcInterceptionResult::kInvalidUrlOrTokens);
  }

  void CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep expected_last_funnel_step,
      std::optional<OidcInterceptionResult> expected_enrollment_result) {
    histogram_tester_.ExpectBucketCount(
        base::StrCat({kOidcEnrollmentHistogramName, ".Interception.Funnel"}),
        expected_last_funnel_step, 1);

    if (expected_enrollment_result == std::nullopt) {
      return;
    }

    histogram_tester_.ExpectUniqueSample(
        base::StrCat({kOidcEnrollmentHistogramName, ".Interception.Result"}),
        expected_enrollment_result.value(), 1);
  }

  void ExpectNoUkmLogged() const {
    EXPECT_TRUE(
        test_ukm_recorder_->GetEntriesByName(kProfileEnrollmentUkm).empty());
  }

  void ExpectUkmLogged(uint64_t navigation_id) const {
    const auto& entries =
        test_ukm_recorder_->GetEntriesByName(kProfileEnrollmentUkm);
    ASSERT_EQ(entries.size(), 1U);
    const auto& entry = entries[0];
    ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
        entry,
        ukm::builders::Enterprise_Profile_Enrollment::
            kIsUntrustedOidcRedirectName,
        true);

    EXPECT_EQ(entry->source_id,
              ukm::ConvertToSourceId(navigation_id,
                                     ukm::SourceIdType::NAVIGATION_ID));
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  bool enable_oidc_interception() { return std::get<0>(GetParam()); }
  bool enable_generic_oidc() { return std::get<1>(GetParam()); }
  bool enable_process_response() { return std::get<2>(GetParam()); }

 protected:
  std::unique_ptr<data_decoder::test::InProcessDataDecoder>
      in_process_data_decoder_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       SuccessfulInterception) {
  TestInterceptionForUrl(/*add_oidc_state=*/false,
                         /*should_log_ukm=*/false,
                         /*source_url=*/kOidcEntraReprocessUrl);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kSuccessfulInfoParsed, std::nullopt);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       SuccessfulHeaderInterception) {
  TestHeaderInterceptionForUrl(kHeaderInterceptionTestUrl);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kSuccessfulInfoParsed, std::nullopt);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       HeaderInterceptionInvalidUrl) {
  content::MockNavigationHandle navigation_handle(
      GURL("https://invalidurl/register"), main_frame());
  navigation_handle.set_response_headers(BuildExampleResponseHeader());

  auto* oidc_interceptor = GetMockOidcInterceptor();
  EXPECT_CALL(*oidc_interceptor,
              MaybeInterceptOidcAuthentication(_, _, _, _, _, _))
      .Times(0);

  content::MockNavigationThrottleRegistry registry(
      &navigation_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = registry.throttles().back().get();

  EXPECT_EQ(NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());

  // Make sure no histogram is being recorded for mismatched URL.
  EXPECT_EQ(histogram_tester_.GetTotalSum(base::StrCat(
                {kOidcEnrollmentHistogramName, ".Interception.Funnel"})),
            0);
  EXPECT_EQ(histogram_tester_.GetTotalSum(base::StrCat(
                {kOidcEnrollmentHistogramName, ".Interception.Result"})),
            0);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       HeaderInterceptionMissingHeader) {
  ExpectHeadersInvalid(nullptr);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       HeaderInterceptionMissingIss) {
  ExpectHeadersInvalid(BuildExampleResponseHeader(
      /*issuer=*/std::string()));
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       HeaderInterceptionMissingSub) {
  ExpectHeadersInvalid(BuildExampleResponseHeader(
      /*issuer=*/kExampleIdIssuer,
      /*subject_id=*/std::string()));
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       HeaderInterceptionMissingCode) {
  ExpectHeadersInvalid(BuildExampleResponseHeader(
      /*issuer=*/kExampleIdIssuer,
      /*subject_id=*/kExampleIdSubject,
      /*encoded_user_info=*/std::string()));
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       SuccessfulInterceptionWithState) {
  TestInterceptionForUrl(/*add_oidc_state=*/true,
                         /*should_log_ukm=*/false,
                         /*source_url=*/kOidcEntraReprocessUrl);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kSuccessfulInfoParsed, std::nullopt);
  ExpectNoUkmLogged();
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       MsftKmsiThrottling) {
  TestInterceptionForUrl(/*add_oidc_state=*/false,
                         /*should_log_ukm=*/false,
                         /*source_url=*/kOidcEntraKmsiUrl);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kSuccessfulInfoParsed, std::nullopt);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       MsftGuidThrottling) {
  TestInterceptionForUrl(
      /*add_oidc_state=*/false,
      /*should_log_ukm=*/false,
      /*source_url=*/
      "https://login.microsoftonline.com/some-tenant-id/login");
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kSuccessfulInfoParsed, std::nullopt);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       McasThrottling) {
  TestInterceptionForUrl(
      /*add_oidc_state=*/false,
      /*should_log_ukm=*/false, /*source_url=*/
      "https://come-dmain-com.access.mcas.ms/aad_login?some-query");
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kSuccessfulInfoParsed, std::nullopt);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       MissingAuthToken) {
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject)
          .Set(kIssuerClaimName, kExampleIdIssuer));

  std::string redirection_url = BuildOidcResponseUrl(
      /*oidc_auth_token=*/std::string(), id_token,
      /*oidc_state=*/std::string());

  auto* oidc_interceptor = GetMockOidcInterceptor();
  RunThrottleAndExpectNoOidcInterception(oidc_interceptor, redirection_url,
                                         NavigationThrottle::PROCEED);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       MissingIdToken) {
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));

  std::string redirection_url =
      BuildOidcResponseUrl(auth_token, /*oidc_id_token=*/std::string(),
                           /*oidc_state=*/std::string());

  auto* oidc_interceptor = GetMockOidcInterceptor();
  RunThrottleAndExpectNoOidcInterception(oidc_interceptor, redirection_url,
                                         NavigationThrottle::PROCEED);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       MissingIdTokenSubClaim) {
  std::string auth_token = BuildTokenFromDict(base::Value::Dict().Set(
      kUserPrincipleNameClaimName, kExampleUserPrincipleName));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kIssuerClaimName, kExampleIdIssuer));

  std::string redirection_url =
      BuildOidcResponseUrl(auth_token, id_token, /*oidc_state=*/std::string());

  auto* oidc_interceptor = GetMockOidcInterceptor();
  RunThrottleAndExpectNoOidcInterception(oidc_interceptor, redirection_url,
                                         NavigationThrottle::DEFER);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       MissingIdTokenIssClaim) {
  std::string auth_token = BuildTokenFromDict(base::Value::Dict().Set(
      kUserPrincipleNameClaimName, kExampleUserPrincipleName));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject));

  std::string redirection_url =
      BuildOidcResponseUrl(auth_token, id_token, /*oidc_state=*/std::string());

  auto* oidc_interceptor = GetMockOidcInterceptor();
  RunThrottleAndExpectNoOidcInterception(oidc_interceptor, redirection_url,
                                         NavigationThrottle::DEFER);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       EmptyIdJson) {
  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict().Set(kSubjectClaimName, kExampleAuthSubject));
  std::string id_token = BuildTokenFromDict(base::Value::Dict());

  std::string redirection_url =
      BuildOidcResponseUrl(auth_token, id_token, /*oidc_state=*/std::string());

  auto* oidc_interceptor = GetMockOidcInterceptor();
  RunThrottleAndExpectNoOidcInterception(oidc_interceptor, redirection_url,
                                         NavigationThrottle::DEFER);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
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

  std::string redirection_url = BuildOidcResponseUrl(
      auth_token, malformed_id_token, /*oidc_state=*/std::string());

  auto* oidc_interceptor = GetMockOidcInterceptor();
  RunThrottleAndExpectNoOidcInterception(oidc_interceptor, redirection_url,
                                         NavigationThrottle::CANCEL_AND_IGNORE);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       DecodeFailure) {
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

  std::string redirection_url = BuildOidcResponseUrl(
      auth_token, malformed_id_token, /*oidc_state=*/std::string());

  auto* oidc_interceptor = GetMockOidcInterceptor();
  RunThrottleAndExpectNoOidcInterception(oidc_interceptor, redirection_url,
                                         NavigationThrottle::CANCEL_AND_IGNORE);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kValidRedirectionCaptured,
      OidcInterceptionResult::kInvalidUrlOrTokens);
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       NoServiceForGuestMode) {
  Browser* guest_browser = CreateGuestBrowser();
  ASSERT_NE(guest_browser, nullptr);
  TestNoServiceForInvalidProfile(guest_browser->GetProfile());
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       NoServiceForIncognito) {
  TestNoServiceForInvalidProfile(browser()->profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true));
}

IN_PROC_BROWSER_TEST_P(OidcAuthResponseCaptureNavigationThrottleTest,
                       NotInMainFrame) {
  GURL test_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_iframe_in_div.html"));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  ASSERT_TRUE(main_frame()->IsRenderFrameLive()) << "Main frame is not live!";

  content::RenderFrameHost* child = GetChild(*main_frame());
  ASSERT_TRUE(child);

  content::MockNavigationHandle navigation_handle(GURL(kOidcEntraReprocessUrl),
                                                  child);

  content::MockNavigationThrottleRegistry registry(
      &navigation_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
  EXPECT_EQ(0u, registry.throttles().size());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OidcAuthResponseCaptureNavigationThrottleTest,
    testing::Combine(/*enable_oidc_interception=*/testing::Values(true),
                     /*enable_generic_oidc=*/testing::Values(false),
                     /*enable_process_response=*/testing::Values(false)));

// Test class dedicated to test if OIDC throttle validatation accepts the
// correct set of URLs.
class OidcAuthNavigationThrottleGenericOidcTest
    : public OidcAuthResponseCaptureNavigationThrottleTest {
 public:
  OidcAuthNavigationThrottleGenericOidcTest() = default;

  ~OidcAuthNavigationThrottleGenericOidcTest() override = default;
};

// Test case for when the source URL of OIDC authentication is not considered
// to be valid. We should only consider interception if Generic OIDC flag is
// enabled.
IN_PROC_BROWSER_TEST_P(OidcAuthNavigationThrottleGenericOidcTest,
                       SuccessfulInterceptionWithState_invalidSourceUrl) {
  TestInterceptionForUrl(
      /*add_oidc_state=*/true,
      /*should_log_ukm=*/!enable_generic_oidc(),
      /*source_url=*/kOidcNonEntraReprocessUrl);

  if (enable_generic_oidc()) {
    CheckFunnelAndResultHistogram(
        OidcInterceptionFunnelStep::kSuccessfulInfoParsed, std::nullopt);
  }
}

IN_PROC_BROWSER_TEST_P(OidcAuthNavigationThrottleGenericOidcTest,
                       MissingRedirectionChain) {
  base::RunLoop run_loop;

  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject)
          .Set(kIssuerClaimName, kExampleIdIssuer));
  std::string url_without_redirection_chain =
      BuildOidcResponseUrl(auth_token, id_token, /*oidc_state=*/std::string());

  content::MockNavigationHandle navigation_handle(
      GURL(url_without_redirection_chain), main_frame());
  auto* oidc_interceptor = GetMockOidcInterceptor();

  if (enable_generic_oidc()) {
    ExpectOidcInterception(
        oidc_interceptor, ProfileManagementOidcTokens(auth_token, id_token,
                                                      /*state=*/std::string()));
  } else {
    EXPECT_CALL(*oidc_interceptor,
                MaybeInterceptOidcAuthentication(_, _, _, _, _, _))
        .Times(0);
  }

  content::MockNavigationThrottleRegistry registry(
      &navigation_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = registry.throttles().back().get();

  if (enable_generic_oidc()) {
    throttle->set_resume_callback_for_testing(run_loop.QuitClosure());
    EXPECT_EQ(NavigationThrottle::DEFER,
              throttle->WillRedirectRequest().action());
    run_loop.Run();
  } else {
    EXPECT_EQ(NavigationThrottle::PROCEED,
              throttle->WillRedirectRequest().action());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OidcAuthNavigationThrottleGenericOidcTest,
    testing::Combine(/*enable_oidc_interception=*/testing::Values(true),
                     /*enable_generic_oidc=*/testing::Bool(),
                     /*enable_process_response=*/testing::Values(false)));

// Test class dedicated to test if OIDC throttle validatation accepts the
// correct set of URLs.
class OidcAuthNavigationThrottleFeatureDisabledTest
    : public OidcAuthResponseCaptureNavigationThrottleTest {
 public:
  OidcAuthNavigationThrottleFeatureDisabledTest() = default;

  ~OidcAuthNavigationThrottleFeatureDisabledTest() override = default;
};

IN_PROC_BROWSER_TEST_P(OidcAuthNavigationThrottleFeatureDisabledTest,
                       NoThrottleCreation) {
  content::MockNavigationHandle msft_navigation_handle(
      GURL(kOidcEntraReprocessUrl), main_frame());
  content::MockNavigationThrottleRegistry msft_registry(
      &msft_navigation_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(msft_registry);
  ASSERT_EQ(0u, msft_registry.throttles().size());

  content::MockNavigationHandle full_navigation_handle(
      GURL(kOidcEntraReprocessUrl), main_frame());
  content::MockNavigationThrottleRegistry full_registry(
      &full_navigation_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  std::string redirection_url =
      BuildStandardResponseUrl(/*oidc_state=*/std::string());
  SetupRedirectionForHandle(
      full_navigation_handle,
      {GURL(kOidcEntraReprocessUrl), GURL(redirection_url)},
      GURL(redirection_url));
  OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(full_registry);
  ASSERT_EQ(0u, full_registry.throttles().size());

  ExpectNoUkmLogged();
}

IN_PROC_BROWSER_TEST_P(OidcAuthNavigationThrottleFeatureDisabledTest,
                       NoInterceptorCreation) {
  ASSERT_EQ(nullptr, GetMockOidcInterceptor());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OidcAuthNavigationThrottleFeatureDisabledTest,
    testing::Combine(/*enable_oidc_interception=*/testing::Values(false),
                     /*enable_generic_oidc=*/testing::Values(false),
                     /*enable_process_response=*/testing::Values(false)));

// Test class dedicated for OIDC throttle regarding WillProcessResponse
class OidcAuthNavigationThrottleProcessResponseTest
    : public OidcAuthResponseCaptureNavigationThrottleTest {
 public:
  OidcAuthNavigationThrottleProcessResponseTest()
      : OidcAuthResponseCaptureNavigationThrottleTest() {}

  ~OidcAuthNavigationThrottleProcessResponseTest() override = default;
};

// Direction navigation should not trigger OIDC enrollment regardless of
// whether ProcessResponse is enabled
IN_PROC_BROWSER_TEST_P(OidcAuthNavigationThrottleProcessResponseTest,
                       NoInterceptionForDirectNavigation) {
  std::string direct_navigate_url =
      BuildStandardResponseUrl(/*oidc_state=*/std::string());

  content::MockNavigationHandle navigation_handle(GURL(direct_navigate_url),
                                                  main_frame());
  auto* oidc_interceptor = GetMockOidcInterceptor();
  EXPECT_CALL(*oidc_interceptor,
              MaybeInterceptOidcAuthentication(_, _, _, _, _, _))
      .Times(0);

  content::MockNavigationThrottleRegistry registry(
      &navigation_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = registry.throttles().back().get();

  navigation_handle.set_url(GURL(direct_navigate_url));
  EXPECT_EQ(NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());

  if (enable_process_response()) {
    ExpectUkmLogged(navigation_handle.GetNavigationId());
  } else {
    ExpectNoUkmLogged();
  }
}

IN_PROC_BROWSER_TEST_P(OidcAuthNavigationThrottleProcessResponseTest,
                       ProcessResponse) {
  base::RunLoop run_loop;

  std::string auth_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleAuthSubject));
  std::string id_token = BuildTokenFromDict(
      base::Value::Dict()
          .Set(kUserPrincipleNameClaimName, kExampleUserPrincipleName)
          .Set(kSubjectClaimName, kExampleIdSubject)
          .Set(kIssuerClaimName, kExampleIdIssuer));

  std::string direct_navigate_url =
      BuildOidcResponseUrl(auth_token, id_token, /*oidc_state=*/std::string());

  content::MockNavigationHandle navigation_handle(GURL(direct_navigate_url),
                                                  main_frame());
  auto* oidc_interceptor = GetMockOidcInterceptor();

  if (enable_process_response()) {
    ExpectOidcInterception(
        oidc_interceptor,
        ProfileManagementOidcTokens(auth_token, id_token, /*state=*/""));
  } else {
    EXPECT_CALL(*oidc_interceptor,
                MaybeInterceptOidcAuthentication(_, _, _, _, _, _))
        .Times(0);
  }

  content::MockNavigationThrottleRegistry registry(
      &navigation_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = registry.throttles().back().get();

  if (enable_process_response()) {
    throttle->set_resume_callback_for_testing(run_loop.QuitClosure());

    SetupRedirectionForHandle(
        navigation_handle,
        {GURL(kOidcEntraReprocessUrl), GURL(direct_navigate_url)},
        GURL(direct_navigate_url));

    EXPECT_EQ(NavigationThrottle::DEFER,
              throttle->WillProcessResponse().action());
    run_loop.Run();
    CheckFunnelAndResultHistogram(
        OidcInterceptionFunnelStep::kSuccessfulInfoParsed, std::nullopt);
  } else {
    navigation_handle.set_url(GURL(direct_navigate_url));
    EXPECT_EQ(NavigationThrottle::PROCEED,
              throttle->WillProcessResponse().action());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OidcAuthNavigationThrottleProcessResponseTest,
    testing::Combine(/*enable_oidc_interception=*/testing::Values(true),
                     /*enable_generic_oidc=*/testing::Values(false),
                     /*enable_process_response=*/testing::Bool()));

// Test class dedicated to test if OIDC throttle validatation accepts the
// correct set of URLs.
class OidcAuthNavigationThrottleUrlMatchingTest
    : public OidcAuthResponseCaptureNavigationThrottleTest {
 public:
  OidcAuthNavigationThrottleUrlMatchingTest()
      : OidcAuthResponseCaptureNavigationThrottleTest(
            /*additional_hosts=*/
            "https://add-host1.com, https://add-host2.com, add-host3.com") {}

  ~OidcAuthNavigationThrottleUrlMatchingTest() override = default;

  void TestUrlMatching(std::string url_string, bool expect_matched = true) {
    ASSERT_EQ(expect_matched, IsSourceUrlValid(url_string));
  }
};

IN_PROC_BROWSER_TEST_P(OidcAuthNavigationThrottleUrlMatchingTest,
                       MsftThrottleUrlMatching) {
  TestUrlMatching("https://login.microsoftonline.com/some-tenant-id/reprocess");
  TestUrlMatching("https://login.microsoftonline.com/common/reprocess");
  TestUrlMatching("https://login.microsoftonline.com/some-tenant-id/login");
  TestUrlMatching("https://login.microsoftonline.com/common/login");
  TestUrlMatching(
      "https://login.microsoftonline.com/common/reprocess?ctx=random-value",
      /*expect_matched=*/true);
  TestUrlMatching("https://login.microsoftonline.com/kmsi");
  TestUrlMatching(
      "https://something-microsoft-com.access.mcas.ms/"
      "aad_login?random-value");
  TestUrlMatching("https://login.microsoftonline.com/common/somethingelse");
}

IN_PROC_BROWSER_TEST_P(OidcAuthNavigationThrottleUrlMatchingTest,
                       AdditionalHostMatching) {
  TestUrlMatching("https://add-host1.com/some-tenant-id/reprocess");
  TestUrlMatching("https://add-host2.com/common/reprocess");
  TestUrlMatching("https://add-host3.com/common/reprocess");
  TestUrlMatching("http://add-host3.com/common/reprocess");
}

IN_PROC_BROWSER_TEST_P(OidcAuthNavigationThrottleUrlMatchingTest,
                       MsftThrottleUrlNotMatching) {
  TestUrlMatching("https://mismatchhost.microsoftonline.com/common/reprocess",
                  /*expect_matched=*/false);
  TestUrlMatching("https://add-host1.ca/common/reprocess",
                  /*expect_matched=*/false);
  TestUrlMatching("https://add-host4.com/common/reprocess",
                  /*expect_matched=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OidcAuthNavigationThrottleUrlMatchingTest,
    testing::Combine(/*enable_oidc_interception=*/testing::Values(true),
                     /*enable_generic_oidc=*/testing::Values(false),
                     /*enable_process_response=*/testing::Values(false)));

}  // namespace profile_management
