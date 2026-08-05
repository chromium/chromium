// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/one_time_token_service_factory.h"
#include "chrome/browser/glic/actor/glic_actor_test_util.h"
#include "chrome/browser/glic/actor/new_glic_actor_functional_browsertest.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/actor/core/actor_switches.h"
#include "components/actor/core/shared_types.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/one_time_tokens/core/browser/mock_one_time_token_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic::actor {
namespace {

using optimization_guide::proto::AnnotatedPageContent;
using ::testing::_;
using ::testing::NiceMock;

class GlicActorAttemptOtpFillingBrowserTest
    : public GlicActorFunctionalBrowserTestBase {
 public:
  GlicActorAttemptOtpFillingBrowserTest()
      : GlicActorFunctionalBrowserTestBase(
            "./glic_actor_attempt_otp_filling_browsertest.js") {
    scoped_feature_list_.InitWithFeatures(
        {autofill::features::kGlicActorAutofill,
         features::kGlicActorAutofillOneTimePassword,
         ::actor::kGlicActorSkipScreenshot},
        {::features::kInitialWebUI});
  }
  ~GlicActorAttemptOtpFillingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "components/test/data");
    GlicActorFunctionalBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());

    // Allow default calls to Subscribe (e.g. from Autofill OtpManager on
    // Android).
    EXPECT_CALL(GetMockOtpService(), Subscribe(_, _, _, _))
        .WillRepeatedly(
            [](one_time_tokens::OneTimeTokenSource source,
               base::Time expiration,
               one_time_tokens::OneTimeTokenService::Callback callback,
               base::OnceClosure expiration_callback) {
              return one_time_tokens::ExpiringSubscription();
            });
    // Allow default calls to GetRecentOneTimeTokens.
    EXPECT_CALL(GetMockOtpService(), GetRecentOneTimeTokens(_))
        .WillRepeatedly(
            [](one_time_tokens::OneTimeTokenService::Callback callback) {});
    // Allow default calls to GetCachedOneTimeTokens.
    EXPECT_CALL(GetMockOtpService(), GetCachedOneTimeTokens())
        .WillRepeatedly(
            []() { return std::vector<one_time_tokens::OneTimeToken>(); });
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicActorFunctionalBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        ::actor::switches::kAttemptOtpFillingBypassLoginCheck);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
#if BUILDFLAG(IS_ANDROID)
    Profile* profile = Profile::FromBrowserContext(context);
    profile->GetPrefs()->SetBoolean(
        autofill::prefs::kAutofillUsingPlatformAutofill, false);
    profile->GetPrefs()->SetBoolean(
        autofill::prefs::kAutofillThirdPartyPasswordManagersAllowed, false);
#endif
    autofill::OneTimeTokenServiceFactory::GetInstance()
        ->SetTestingSubclassFactoryAndUse<
            one_time_tokens::MockOneTimeTokenService>(
            context,
            base::BindOnce(
                &GlicActorAttemptOtpFillingBrowserTest::CreateMockOtpService));
    AffiliationServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        context, base::BindRepeating(&GlicActorAttemptOtpFillingBrowserTest::
                                         CreateMockAffiliationService));
  }

 protected:
  static std::unique_ptr<KeyedService> CreateMockAffiliationService(
      content::BrowserContext* context) {
    return std::make_unique<
        testing::NiceMock<affiliations::MockAffiliationService>>();
  }

  affiliations::MockAffiliationService& GetMockAffiliationService() {
    auto* service = static_cast<affiliations::MockAffiliationService*>(
        AffiliationServiceFactory::GetForProfile(GetProfile()));
    CHECK(service);
    return *service;
  }

  static std::unique_ptr<one_time_tokens::MockOneTimeTokenService>
  CreateMockOtpService(content::BrowserContext* context) {
    return std::make_unique<
        testing::NiceMock<one_time_tokens::MockOneTimeTokenService>>();
  }

  one_time_tokens::MockOneTimeTokenService& GetMockOtpService() {
    auto* mock_otp_service =
        static_cast<one_time_tokens::MockOneTimeTokenService*>(
            autofill::OneTimeTokenServiceFactory::GetForProfile(GetProfile()));
    CHECK(mock_otp_service);
    return *mock_otp_service;
  }

  // Synchronously fetches APC for the active tab.
  std::unique_ptr<AnnotatedPageContent> FetchApcForTab() {
    base::test::TestFuture<GlicGetContextResult> future;
    auto options = mojom::TabContextOptions::New();
    options->annotated_page_content = true;
    options->annotated_page_content_mode = optimization_guide::proto::
        ANNOTATED_PAGE_CONTENT_MODE_ACTIONABLE_ELEMENTS;

    GetOnlyGlicInstance()
        ->host()
        .GetSharingManagerInternal()
        .GetContextForActorFromTab(active_tab()->GetHandle(), *options.get(),
                                   future.GetCallback());

    GlicGetContextResult result = future.Take();
    if (!result.has_value()) {
      return nullptr;
    }

    mojo_base::ProtoWrapper& serialized_apc =
        *result.value()
             ->get_tab_context()
             ->annotated_page_data->annotated_page_content;
    return std::make_unique<AnnotatedPageContent>(
        serialized_apc.As<AnnotatedPageContent>().value());
  }

  // Helper to handle the common first step of the test:
  // 1. Initialize prefs.
  // 2. Wait for TS to create task and yield.
  // 3. Navigate tab to OTP page.
  // 4. Fetch APC and find OTP field.
  // 5. Continue TS with node details.
  void SetupTestAndSubmitOtpField() {
    PrefService* prefs = GetProfile()->GetPrefs();
    autofill::prefs::SetAutofillGmailOtpFillingEnabled(prefs, false);
    autofill::prefs::ClearAutofillGmailOtpFillingActivationDismissalTimestamp(
        prefs);

    ExecuteJsTest();

    // TS has created the task and yielded.
    content::WebContents* tab = active_tab()->GetContents();
    ASSERT_TRUE(content::NavigateToURL(tab, GetOtpPageUrl()));

    // Wait for rendering to sync.
    {
      base::test::TestFuture<bool> future;
      tab->GetPrimaryMainFrame()
          ->GetRenderWidgetHost()
          ->InsertVisualStateCallback(future.GetCallback());
      ASSERT_TRUE(future.Wait());
    }

    std::unique_ptr<AnnotatedPageContent> apc = FetchApcForTab();
    ASSERT_TRUE(apc);

    base::flat_map<std::string, ::actor::DomNode> label_map =
        BuildFormLabelsMap(*apc);
    ASSERT_TRUE(label_map.contains("One-Time Password:"));
    ::actor::DomNode otp_node = label_map["One-Time Password:"];

    base::DictValue params;
    params.Set("nodeId", otp_node.node_id);
    params.Set("documentIdentifier", otp_node.document_identifier);

    ContinueJsTest({.params = base::Value(std::move(params))});
  }

 private:
  GURL GetOtpPageUrl() {
    return embedded_https_test_server().GetURL("example.com",
                                               "/actor/otp_page.html");
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorAttemptOtpFillingBrowserTest,
                       testAllTestsAreRegistered) {
  AssertAllTestsRegistered({"GlicActorAttemptOtpFillingBrowserTest"});
}

IN_PROC_BROWSER_TEST_F(GlicActorAttemptOtpFillingBrowserTest,
                       testOptInDeclined) {
  // No mock OTP setup needed because user declines opt-in, so retrieve is not
  // called.
  SetupTestAndSubmitOtpField();

  // Wait for TS to finish (it will perform actions, decline dialog, assert
  // result, and stop). No second C++ yield expected for this test case because
  // it fails.
}

IN_PROC_BROWSER_TEST_F(GlicActorAttemptOtpFillingBrowserTest,
                       testOptInAccepted) {
  PrefService* prefs = GetProfile()->GetPrefs();

  // Mock Affiliation Service.
  EXPECT_CALL(GetMockAffiliationService(), GetPSLExtensions(_))
      .WillOnce(
          [](base::OnceCallback<void(std::vector<std::string>)> callback) {
            std::move(callback).Run({});
          });
  EXPECT_CALL(GetMockAffiliationService(), GetGroupingInfo(_, _))
      .WillOnce([](std::vector<affiliations::FacetURI> facets,
                   affiliations::AffiliationService::GroupsCallback callback) {
        std::vector<affiliations::GroupedFacets> groups;
        for (const auto& facet : facets) {
          affiliations::GroupedFacets group;
          group.facets.push_back(affiliations::Facet{facet});
          groups.push_back(group);
        }
        std::move(callback).Run(groups);
      });
  EXPECT_CALL(GetMockAffiliationService(), GetAffiliationsAndBranding(_, _))
      .WillOnce([](const affiliations::FacetURI& facet_uri,
                   affiliations::AffiliationService::ResultCallback callback) {
        affiliations::AffiliatedFacets affiliated_facets;
        affiliated_facets.push_back(affiliations::Facet{
            affiliations::FacetURI::FromCanonicalSpec("https://example.com")});
        affiliated_facets.push_back(affiliations::Facet{facet_uri});
        std::move(callback).Run(affiliated_facets, /*success=*/true);
      });

  // Mock OTP Service to succeed.
  EXPECT_CALL(GetMockOtpService(),
              Subscribe(one_time_tokens::OneTimeTokenSource::kGmail, _, _, _))
      .WillOnce([](one_time_tokens::OneTimeTokenSource source,
                   base::Time expiration,
                   one_time_tokens::OneTimeTokenService::Callback callback,
                   base::OnceClosure expiration_callback) {
        std::move(callback).Run(
            one_time_tokens::OneTimeTokenSource::kGmail,
            base::expected<one_time_tokens::OneTimeToken,
                           one_time_tokens::OneTimeTokenRetrievalError>(
                one_time_tokens::OneTimeToken(
                    one_time_tokens::OneTimeTokenType::kGmail, "123456",
                    base::TimeTicks::Now(), "sender@example.com")));
        return one_time_tokens::ExpiringSubscription();
      });

  SetupTestAndSubmitOtpField();

  // SetupTestAndSubmitOtpField() blocks until the JS test performs the actions
  // and yields back to C++ (second yield) after successful form filling.

  // Now we verify in C++:
  // 1. Prefs should be enabled.
  EXPECT_TRUE(autofill::prefs::IsAutofillGmailOtpFillingEnabled(prefs));
  EXPECT_TRUE(
      autofill::prefs::GetAutofillGmailOtpFillingActivationDismissalTimestamp(
          prefs)
          .is_null());

  // 2. Field should be filled.
  content::WebContents* tab = active_tab()->GetContents();
  EXPECT_EQ("123456",
            content::EvalJs(tab, "document.getElementById('otp').value"));

  // Continue TS to let it stop the task and finish.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicActorAttemptOtpFillingBrowserTest,
                       testOptInAcceptedButRetrievalFails) {
  // Mock OTP Service to fail.
  EXPECT_CALL(GetMockOtpService(),
              Subscribe(one_time_tokens::OneTimeTokenSource::kGmail, _, _, _))
      .WillOnce([](one_time_tokens::OneTimeTokenSource source,
                   base::Time expiration,
                   one_time_tokens::OneTimeTokenService::Callback callback,
                   base::OnceClosure expiration_callback) {
        std::move(callback).Run(
            one_time_tokens::OneTimeTokenSource::kGmail,
            base::unexpected(one_time_tokens::OneTimeTokenRetrievalError::
                                 kGmailOtpBackendServerError));
        return one_time_tokens::ExpiringSubscription();
      });

  SetupTestAndSubmitOtpField();

  // TS performs actions, accepts dialog, performActions fails with retrieval
  // error. TS asserts this and then stops the task. No second C++ yield
  // expected because expectSuccess is false.
}

}  // namespace
}  // namespace glic::actor
