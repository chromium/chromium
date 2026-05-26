// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/oidc_auth_response_capture_navigation_throttle.h"

#include <memory>

#include "base/base64url.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::FeatureRef;
using base::test::FeatureRefAndParams;
using content::NavigationThrottle;
using testing::_;

namespace {

constexpr char kHeaderInterceptionTestUrl[] =
    "https://chromeenterprise.google/profile-enrollment/register-handler";

constexpr char kExampleIdSubject[] = "example_id_subject";
constexpr char kExampleIdIssuer[] = "example_id_issuer";
constexpr char kExampleEncodedInfo[] = "EncodedMessageInBase64";

constexpr char kOidcEnrollmentHistogramName[] = "Enterprise.OidcEnrollment";

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
    : public InProcessBrowserTest {
 public:
  OidcAuthResponseCaptureNavigationThrottleTest()
      : OidcAuthResponseCaptureNavigationThrottleTest(true) {}

  ~OidcAuthResponseCaptureNavigationThrottleTest() override = default;

 protected:
  explicit OidcAuthResponseCaptureNavigationThrottleTest(
      bool enable_oidc_interception) {
    std::vector<FeatureRefAndParams> enabled_features;
    std::vector<FeatureRef> disabled_features;

    ExpandFeatureList(
        enabled_features, disabled_features,
        {{features::kOidcAuthProfileManagement, enable_oidc_interception}});

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

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
    navigation_handle.set_url(GURL(kHeaderInterceptionTestUrl));
    navigation_handle.set_response_headers(BuildExampleResponseHeader());

    ASSERT_EQ(nullptr, oidc_interceptor);

    content::MockNavigationThrottleRegistry registry(
        &navigation_handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
    ASSERT_EQ(1u, registry.throttles().size());
    auto* throttle = registry.throttles().back().get();

    EXPECT_EQ(NavigationThrottle::PROCEED,
              throttle->WillProcessResponse().action());

    histogram_tester_.ExpectTotalCount(
        base::StrCat({kOidcEnrollmentHistogramName, ".Interception.Funnel"}),
        0);

    histogram_tester_.ExpectUniqueSample(
        base::StrCat({kOidcEnrollmentHistogramName, ".Interception.Result"}),
        OidcInterceptionResult::kInvalidProfile, 1);
  }

  void TestHeaderInterceptionForUrl(std::string source_url) {
    base::RunLoop run_loop;

    content::MockNavigationHandle navigation_handle(GURL(source_url),
                                                    main_frame());
    navigation_handle.set_response_headers(BuildExampleResponseHeader());

    auto* oidc_interceptor = GetMockOidcInterceptor();
    ExpectOidcInterception(oidc_interceptor,
                           ProfileManagementOidcTokens(kExampleEncodedInfo));

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

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OidcAuthResponseCaptureNavigationThrottleTest,
                       SuccessfulHeaderInterception) {
  TestHeaderInterceptionForUrl(kHeaderInterceptionTestUrl);
  CheckFunnelAndResultHistogram(
      OidcInterceptionFunnelStep::kSuccessfulInfoParsed, std::nullopt);
}

IN_PROC_BROWSER_TEST_F(OidcAuthResponseCaptureNavigationThrottleTest,
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

IN_PROC_BROWSER_TEST_F(OidcAuthResponseCaptureNavigationThrottleTest,
                       HeaderInterceptionMissingHeader) {
  ExpectHeadersInvalid(nullptr);
}

IN_PROC_BROWSER_TEST_F(OidcAuthResponseCaptureNavigationThrottleTest,
                       HeaderInterceptionMissingIss) {
  ExpectHeadersInvalid(BuildExampleResponseHeader(
      /*issuer=*/std::string()));
}

IN_PROC_BROWSER_TEST_F(OidcAuthResponseCaptureNavigationThrottleTest,
                       HeaderInterceptionMissingSub) {
  ExpectHeadersInvalid(BuildExampleResponseHeader(
      /*issuer=*/kExampleIdIssuer,
      /*subject_id=*/std::string()));
}

IN_PROC_BROWSER_TEST_F(OidcAuthResponseCaptureNavigationThrottleTest,
                       HeaderInterceptionMissingCode) {
  ExpectHeadersInvalid(BuildExampleResponseHeader(
      /*issuer=*/kExampleIdIssuer,
      /*subject_id=*/kExampleIdSubject,
      /*encoded_user_info=*/std::string()));
}

IN_PROC_BROWSER_TEST_F(OidcAuthResponseCaptureNavigationThrottleTest,
                       NoServiceForGuestMode) {
  Browser* guest_browser = CreateGuestBrowser();
  ASSERT_NE(guest_browser, nullptr);
  TestNoServiceForInvalidProfile(guest_browser->GetProfile());
}

IN_PROC_BROWSER_TEST_F(OidcAuthResponseCaptureNavigationThrottleTest,
                       NoServiceForIncognito) {
  TestNoServiceForInvalidProfile(browser()->profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true));
}

IN_PROC_BROWSER_TEST_F(OidcAuthResponseCaptureNavigationThrottleTest,
                       NotInMainFrame) {
  GURL test_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_iframe_in_div.html"));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  ASSERT_TRUE(main_frame()->IsRenderFrameLive()) << "Main frame is not live!";

  content::RenderFrameHost* child = GetChild(*main_frame());
  ASSERT_TRUE(child);

  content::MockNavigationHandle navigation_handle(
      GURL(kHeaderInterceptionTestUrl), child);

  content::MockNavigationThrottleRegistry registry(
      &navigation_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
  EXPECT_EQ(0u, registry.throttles().size());
}

class OidcAuthNavigationThrottleFeatureDisabledTest
    : public OidcAuthResponseCaptureNavigationThrottleTest {
 public:
  OidcAuthNavigationThrottleFeatureDisabledTest()
      : OidcAuthResponseCaptureNavigationThrottleTest(false) {}

  ~OidcAuthNavigationThrottleFeatureDisabledTest() override = default;
};

IN_PROC_BROWSER_TEST_F(OidcAuthNavigationThrottleFeatureDisabledTest,
                       NoThrottleCreation) {
  content::MockNavigationHandle navigation_handle(
      GURL(kHeaderInterceptionTestUrl), main_frame());
  content::MockNavigationThrottleRegistry registry(
      &navigation_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  OidcAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(0u, registry.throttles().size());
}

IN_PROC_BROWSER_TEST_F(OidcAuthNavigationThrottleFeatureDisabledTest,
                       NoInterceptorCreation) {
  ASSERT_EQ(nullptr, GetMockOidcInterceptor());
}

}  // namespace profile_management
