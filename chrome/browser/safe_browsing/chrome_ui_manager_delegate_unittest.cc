// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#endif

namespace safe_browsing {

namespace {

constexpr char kPreviousPageTitle[] = "Wikipedia";
constexpr char kInterstitialTitle[] = "Security Error";
constexpr char kDestinationTitle[] = "Malicious Title";

constexpr char kPreviousPageUrl[] = "https://wikipedia.example/";
constexpr char kBlockedUrl[] = "https://blocked.example/";
constexpr char kInterstitialUrl[] = "https://interstitial.example/";
constexpr char kDestinationUrl[] = "https://malicious.example/";

safe_browsing::RTLookupResponse MakeManagedPolicyResponse() {
  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  threat_info->set_threat_type(
      safe_browsing::RTLookupResponse::ThreatInfo::MANAGED_POLICY);
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  auto* rule = threat_info->mutable_matched_url_navigation_rule();
  rule->set_rule_id("test_rule_id");
  rule->set_rule_name("test_rule_name");
  rule->set_matched_url_category("test_category");
  return response;
}

}  // namespace

class ChromeSafeBrowsingUIManagerDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeSafeBrowsingUIManagerDelegateTest() = default;
  ~ChromeSafeBrowsingUIManagerDelegateTest() override = default;
};

// Verifies that ChromeSafeBrowsingUIManagerDelegate attributes `tab_title` to
// the page the event is actually about. These events fire at moments when
// WebContents::GetTitle() describes a *different* document: "*_SEEN" / "*Shown"
// fires from SafeBrowsingUIManager::CreateBlockingPage() before the interstitial
// commits, and "*_BYPASS" / "*Proceeded" fires while the interstitial is still
// the committed page.
class ChromeSafeBrowsingUIManagerDelegateTabTitleTest
    : public ChromeSafeBrowsingUIManagerDelegateTest {
 public:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    ChromeSafeBrowsingUIManagerDelegateTest::SetUp();

    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm-token"));
    client_ = std::make_unique<policy::MockCloudPolicyClient>();

    enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
        ->SetTestingFactory(
            profile(),
            base::BindRepeating(
                &enterprise_connectors::test::BuildRealtimeReportingClient));
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile())
        ->SetBrowserCloudPolicyClientForTesting(client_.get());
    identity_test_environment_.MakePrimaryAccountAvailable(
        "test-user@chromium.org", signin::ConsentLevel::kSignin);
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile())
        ->SetIdentityManagerForTesting(
            identity_test_environment_.identity_manager());

    enterprise_connectors::test::SetOnSecurityEventReporting(
        profile()->GetPrefs(), /*enabled=*/true);

#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            profile(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  extensions::SafeBrowsingPrivateEventRouter>(context);
            }));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  }

  void TearDown() override {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(profile(),
                            BrowserContextKeyedServiceFactory::TestingFactory());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile())
        ->SetBrowserCloudPolicyClientForTesting(nullptr);
    ChromeSafeBrowsingUIManagerDelegateTest::TearDown();
    profile_manager_.reset();
  }

  // Commits `url` and sets `title` on the committed page.
  void CommitPageWithTitle(const std::string& url,
                           const std::u16string& title) {
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(url), web_contents()->GetPrimaryMainFrame());
    simulator->Start();
    content::WebContentsTester::For(web_contents())->SetTitle(title);
    simulator->Commit();
  }

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent ExpectedEvent(
      const std::string& url,
      const std::string& threat_type,
      const safe_browsing::RTLookupResponse& response,
      const std::string& tab_title) {
    auto* reporting_client =
        enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
            profile());
    return enterprise_connectors::GetUrlFilteringInterstitialEvent(
        GURL(url), threat_type, response,
        reporting_client->GetProfileIdentifier(),
        reporting_client->GetProfileUserName(),
        /*active_user=*/std::string(),
        /*referrer_chain=*/enterprise_connectors::ReferrerChain(), tab_title);
  }

  chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent
  ExpectedSecurityInterstitialEvent(
      const std::string& url,
      const std::string& reason,
      int net_error_code,
      bool clicked_through,
      enterprise_connectors::EventResult event_result,
      const std::string& tab_title) {
    auto* reporting_client =
        enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
            profile());
    return enterprise_connectors::GetInterstitialEvent(
        GURL(url), reason, net_error_code, clicked_through, event_result,
        reporting_client->GetProfileIdentifier(),
        reporting_client->GetProfileUserName(),
        /*referrer_chain=*/enterprise_connectors::ReferrerChain(), tab_title);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  signin::IdentityTestEnvironment identity_test_environment_;
  ChromeSafeBrowsingUIManagerDelegate delegate_;
};

TEST_F(ChromeSafeBrowsingUIManagerDelegateTest, IsHostingExtension) {
  ChromeSafeBrowsingUIManagerDelegate delegate;

  // Sanity-check that vanilla WebContents instances are not marked as hosting
  // extensions.
  EXPECT_FALSE(delegate.IsHostingExtension(web_contents()));

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Create a WebContents instance that *is* hosting an extension.
  base::DictValue manifest;
  manifest.Set(extensions::manifest_keys::kName, "TestComponentApp");
  manifest.Set(extensions::manifest_keys::kVersion, "0.0.0.0");
  manifest.SetByDottedPath(
      extensions::manifest_keys::kPlatformAppBackgroundPage, "background.html");
  std::u16string error;
  scoped_refptr<extensions::Extension> app;
  app = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kComponent,
      manifest, 0, &error);
  ASSERT_TRUE(app) << error;
  extensions::ProcessManager* extension_manager =
      extensions::ProcessManager::Get(web_contents()->GetBrowserContext());
  extension_manager->CreateBackgroundHost(app.get(), GURL("background.html"));
  extensions::ExtensionHost* host =
      extension_manager->GetBackgroundHostForExtension(app->id());
  auto* extension_web_contents = host->host_contents();

  // Check that the delegate flags this WebContents as hosting an extension.
  EXPECT_TRUE(delegate.IsHostingExtension(extension_web_contents));

  delete host;
#endif
}

// Tests that blocking/warning a URL does not report the title of the previous
// page if one is already open.
TEST_F(ChromeSafeBrowsingUIManagerDelegateTabTitleTest,
       NotBypassingInterstitialReportsNoTitle) {
  scoped_feature_list_.InitWithFeatures(
      {enterprise_data_protection::kEnterpriseTabTitleReporting},
      {safe_browsing::kEnhancedFieldsForSecOps});

  CommitPageWithTitle(kPreviousPageUrl,
                      base::UTF8ToUTF16(std::string(kPreviousPageTitle)));
  ASSERT_EQ(base::UTF8ToUTF16(std::string(kPreviousPageTitle)),
            web_contents()->GetTitle());

  safe_browsing::RTLookupResponse response = MakeManagedPolicyResponse();

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectUrlFilteringInterstitialEvent(
      ExpectedEvent(kBlockedUrl, "ENTERPRISE_BLOCKED_SEEN", response,
                    /*tab_title=*/std::string()));

  delegate_.TriggerUrlFilteringInterstitialExtensionEventIfDesired(
      web_contents(), GURL(kBlockedUrl), "ENTERPRISE_BLOCKED_SEEN", response,
      /*is_bypassing_interstitial=*/false);

  run_loop.Run();
}

// Tests that bypassing a warning reports the destination page's title, not the
// interstitial's title.
TEST_F(ChromeSafeBrowsingUIManagerDelegateTabTitleTest,
       BypassingInterstitialReportsTitle) {
  scoped_feature_list_.InitWithFeatures(
      {enterprise_data_protection::kEnterpriseTabTitleReporting},
      {safe_browsing::kEnhancedFieldsForSecOps});

  // The interstitial is the committed page when the bypass event fires.
  CommitPageWithTitle(kInterstitialUrl,
                      base::UTF8ToUTF16(std::string(kInterstitialTitle)));
  ASSERT_EQ(base::UTF8ToUTF16(std::string(kInterstitialTitle)),
            web_contents()->GetTitle());

  safe_browsing::RTLookupResponse response = MakeManagedPolicyResponse();

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectUrlFilteringInterstitialEvent(
      ExpectedEvent(kDestinationUrl, "ENTERPRISE_WARNED_BYPASS", response,
                    kDestinationTitle));

  delegate_.TriggerUrlFilteringInterstitialExtensionEventIfDesired(
      web_contents(), GURL(kDestinationUrl), "ENTERPRISE_WARNED_BYPASS",
      response, /*is_bypassing_interstitial=*/true);

  // The user proceeds: the destination page commits and finishes loading.
  CommitPageWithTitle(kDestinationUrl,
                      base::UTF8ToUTF16(std::string(kDestinationTitle)));
  content::WebContentsTester::For(web_contents())
      ->TestDidFinishLoad(GURL(kDestinationUrl));

  run_loop.Run();
}

// Tests that no title is reported when the feature is disabled.
TEST_F(ChromeSafeBrowsingUIManagerDelegateTabTitleTest,
       BypassingInterstitialReportsNoTitleWhenFeatureDisabled) {
  scoped_feature_list_.InitWithFeatures(
      {}, {enterprise_data_protection::kEnterpriseTabTitleReporting,
           safe_browsing::kEnhancedFieldsForSecOps});

  CommitPageWithTitle(kDestinationUrl,
                      base::UTF8ToUTF16(std::string(kInterstitialTitle)));

  safe_browsing::RTLookupResponse response = MakeManagedPolicyResponse();

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectUrlFilteringInterstitialEvent(
      ExpectedEvent(kDestinationUrl, "ENTERPRISE_WARNED_BYPASS", response,
                    /*tab_title=*/std::string()));

  delegate_.TriggerUrlFilteringInterstitialExtensionEventIfDesired(
      web_contents(), GURL(kDestinationUrl), "ENTERPRISE_WARNED_BYPASS",
      response, /*is_bypassing_interstitial=*/true);

  run_loop.Run();
}

// Tests that showing a security interstitial reports no tab title.
TEST_F(ChromeSafeBrowsingUIManagerDelegateTabTitleTest,
       SecurityInterstitialShownReportsNoTitle) {
  scoped_feature_list_.InitWithFeatures(
      {enterprise_data_protection::kEnterpriseTabTitleReporting},
      {safe_browsing::kEnhancedFieldsForSecOps});

  CommitPageWithTitle(kPreviousPageUrl,
                      base::UTF8ToUTF16(std::string(kPreviousPageTitle)));
  ASSERT_EQ(base::UTF8ToUTF16(std::string(kPreviousPageTitle)),
            web_contents()->GetTitle());

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectSecurityInterstitialEvent(
      ExpectedSecurityInterstitialEvent(kBlockedUrl, "MALWARE",
                                        /*net_error_code=*/0,
                                        /*clicked_through=*/false,
                                        enterprise_connectors::EventResult::WARNED,
                                        /*tab_title=*/std::string()));

  delegate_.TriggerSecurityInterstitialShownExtensionEventIfDesired(
      web_contents(), GURL(kBlockedUrl), "MALWARE", /*net_error_code=*/0);

  run_loop.Run();
}

// Tests that proceeding past a security interstitial reports the destination
// page's title after it commits and finishes loading.
TEST_F(ChromeSafeBrowsingUIManagerDelegateTabTitleTest,
       SecurityInterstitialProceededReportsDestinationPageTitle) {
  scoped_feature_list_.InitWithFeatures(
      {enterprise_data_protection::kEnterpriseTabTitleReporting},
      {safe_browsing::kEnhancedFieldsForSecOps});

  CommitPageWithTitle(kInterstitialUrl,
                      base::UTF8ToUTF16(std::string(kInterstitialTitle)));
  ASSERT_EQ(base::UTF8ToUTF16(std::string(kInterstitialTitle)),
            web_contents()->GetTitle());

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectSecurityInterstitialEvent(
      ExpectedSecurityInterstitialEvent(kDestinationUrl, "MALWARE",
                                        /*net_error_code=*/0,
                                        /*clicked_through=*/true,
                                        enterprise_connectors::EventResult::BYPASSED,
                                        kDestinationTitle));

  delegate_.TriggerSecurityInterstitialProceededExtensionEventIfDesired(
      web_contents(), GURL(kDestinationUrl), "MALWARE", /*net_error_code=*/0);

  // The user proceeds: the destination page commits and finishes loading.
  CommitPageWithTitle(kDestinationUrl,
                      base::UTF8ToUTF16(std::string(kDestinationTitle)));
  content::WebContentsTester::For(web_contents())
      ->TestDidFinishLoad(GURL(kDestinationUrl));

  run_loop.Run();
}

}  // namespace safe_browsing
