// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/browser/threat_details.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/safe_browsing_quiet_error_ui.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#endif

using content::InterstitialPage;
using content::NavigationEntry;
using content::NavigationSimulator;
using content::WebContents;
using content::WebContentsTester;
using security_interstitials::BaseSafeBrowsingErrorUI;

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace OnSecurityInterstitialShown =
    extensions::api::safe_browsing_private::OnSecurityInterstitialShown;
namespace OnSecurityInterstitialProceeded =
    extensions::api::safe_browsing_private::OnSecurityInterstitialProceeded;
#endif

static const char* kGoogleURL = "http://www.google.com/";
static const char* kPageURL = "http://www.example.com/";
static const char* kBadURL = "http://www.badguys.com/";
static const char* kBadURL2 = "http://www.badguys2.com/";
static const char* kBadURL3 = "http://www.badguys3.com/";

namespace safe_browsing {

namespace {

// A SafeBrowingBlockingPage class that does not create windows.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(
      BaseUIManager* manager,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResourceList& unsafe_resources,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options)
      : SafeBrowsingBlockingPage(manager,
                                 web_contents,
                                 main_frame_url,
                                 unsafe_resources,
                                 display_options) {
    // Don't delay details at all for the unittest.
    SetThreatDetailsProceedDelayForTesting(0);
    DontCreateViewForTesting();
  }
};

class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() { }
  ~TestSafeBrowsingBlockingPageFactory() override {}

  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* manager,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    PrefService* prefs = profile->GetPrefs();
    bool is_extended_reporting_opt_in_allowed =
        prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed);
    bool is_proceed_anyway_disabled =
        prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled);
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    bool is_unified_consent_given =
        consent_service && consent_service->IsUnifiedConsentGiven();
    BaseSafeBrowsingErrorUI::SBErrorDisplayOptions display_options(
        BaseBlockingPage::IsMainPageLoadBlocked(unsafe_resources),
        is_extended_reporting_opt_in_allowed,
        web_contents->GetBrowserContext()->IsOffTheRecord(),
        is_unified_consent_given, IsExtendedReportingEnabled(*prefs),
        IsExtendedReportingPolicyManaged(*prefs), is_proceed_anyway_disabled,
        true,  // should_open_links_in_new_tab
        true,  // always_show_back_to_safety
        "cpn_safe_browsing" /* help_center_article_link */);
    return new TestSafeBrowsingBlockingPage(manager, web_contents,
                                            main_frame_url, unsafe_resources,
                                            display_options);
  }
};

class MockTestingProfile : public TestingProfile {
 public:
  MockTestingProfile() {}
  ~MockTestingProfile() override {}

  MOCK_CONST_METHOD0(IsOffTheRecord, bool());
};

class TestSafeBrowsingBlockingPageQuiet : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPageQuiet(
      BaseUIManager* manager,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResourceList& unsafe_resources,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options)
      : SafeBrowsingBlockingPage(manager,
                                 web_contents,
                                 main_frame_url,
                                 unsafe_resources,
                                 display_options),
        sb_error_ui_(unsafe_resources[0].url,
                     main_frame_url,
                     GetInterstitialReason(unsafe_resources),
                     display_options,
                     manager->app_locale(),
                     base::Time::NowFromSystemTime(),
                     controller(),
                     false) {
    // Don't delay details at all for the unittest.
    SetThreatDetailsProceedDelayForTesting(0);
    DontCreateViewForTesting();
  }

  // Manually specify that the WebView extends beyond viewing bounds.
  void SetGiantWebView() { sb_error_ui_.SetGiantWebViewForTesting(true); }

  base::DictionaryValue GetUIStrings() {
    base::DictionaryValue load_time_data;
    sb_error_ui_.PopulateStringsForHtml(&load_time_data);
    webui::SetLoadTimeDataDefaults(controller()->GetApplicationLocale(),
                                   &load_time_data);
    return load_time_data;
  }

  security_interstitials::SafeBrowsingQuietErrorUI sb_error_ui_;
};

// TODO(edwardjung): Refactor into TestSafeBrowsingBlockingPageFactory.
class TestSafeBrowsingBlockingQuietPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingQuietPageFactory() {}
  ~TestSafeBrowsingBlockingQuietPageFactory() override {}

  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* manager,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    PrefService* prefs = profile->GetPrefs();
    bool is_extended_reporting_opt_in_allowed =
        prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed);
    bool is_proceed_anyway_disabled =
        prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled);
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    bool is_unified_consent_given =
        consent_service && consent_service->IsUnifiedConsentGiven();
    BaseSafeBrowsingErrorUI::SBErrorDisplayOptions display_options(
        BaseBlockingPage::IsMainPageLoadBlocked(unsafe_resources),
        is_extended_reporting_opt_in_allowed,
        web_contents->GetBrowserContext()->IsOffTheRecord(),
        is_unified_consent_given, IsExtendedReportingEnabled(*prefs),
        IsExtendedReportingPolicyManaged(*prefs), is_proceed_anyway_disabled,
        true,  // should_open_links_in_new_tab
        true,  // always_show_back_to_safety
        "cpn_safe_browsing" /* help_center_article_link */);
    return new TestSafeBrowsingBlockingPageQuiet(
        manager, web_contents, main_frame_url, unsafe_resources,
        display_options);
  }
};

}  // namespace

class SafeBrowsingBlockingPageTest : public ChromeRenderViewHostTestHarness {
 public:
  // The decision the user made.
  enum UserResponse {
    PENDING,
    OK,
    CANCEL
  };

  SafeBrowsingBlockingPageTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {
    ResetUserResponse();
    // The safe browsing UI manager does not need a service for this test.
    ui_manager_ = new TestSafeBrowsingUIManager(NULL);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    system_request_context_getter_ =
        base::MakeRefCounted<net::TestURLRequestContextGetter>(
            base::CreateSingleThreadTaskRunnerWithTraits(
                {content::BrowserThread::IO}));
    TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(
        system_request_context_getter_.get());

    SafeBrowsingBlockingPage::RegisterFactory(&factory_);
    ResetUserResponse();
    SafeBrowsingUIManager::CreateWhitelistForTesting(web_contents());

    safe_browsing::TestSafeBrowsingServiceFactory sb_service_factory;
    sb_service_factory.SetTestUIManager(ui_manager_.get());
    auto* safe_browsing_service =
        sb_service_factory.CreateSafeBrowsingService();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        safe_browsing_service);
    g_browser_process->safe_browsing_service()->Initialize();
    // A profile was created already but SafeBrowsingService wasn't around to
    // get notified of it, so include that notification now.
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    safe_browsing_service->AddPrefService(profile->GetPrefs());
#if BUILDFLAG(ENABLE_EXTENSIONS)
    test_event_router_ = extensions::CreateAndUseTestEventRouter(profile);
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            profile, base::BindRepeating(&BuildSafeBrowsingPrivateEventRouter));
    observer_ =
        std::make_unique<TestExtensionEventObserver>(test_event_router_);
#endif
  }

  void TearDown() override {
    // Release the UI manager before the BrowserThreads are destroyed.
    ui_manager_ = NULL;
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    SafeBrowsingBlockingPage::RegisterFactory(NULL);
    TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(nullptr);
    system_request_context_getter_ = nullptr;
    // Clean up singleton reference (crbug.com/110594).
    ThreatDetails::RegisterFactory(NULL);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::BrowserContext* CreateBrowserContext() override {
    // Set custom profile object so that we can mock calls to IsOffTheRecord.
    // This needs to happen before we call the parent SetUp() function.  We use
    // a nice mock because other parts of the code are calling IsOffTheRecord.
    mock_profile_ = new testing::NiceMock<MockTestingProfile>();
    return mock_profile_;
  }

  void SetProfileOffTheRecord() {
    EXPECT_CALL(*mock_profile_, IsOffTheRecord())
          .WillRepeatedly(testing::Return(true));
  }

  void OnBlockingPageComplete(bool proceed) {
    if (proceed)
      user_response_ = OK;
    else
      user_response_ = CANCEL;
  }

  void ShowInterstitial(bool is_subresource, const char* url) {
    ShowInterstitial(is_subresource, url, SB_THREAT_TYPE_URL_MALWARE);
  }

  void ShowInterstitial(bool is_subresource,
                        const char* url,
                        SBThreatType type) {
    security_interstitials::UnsafeResource resource;
    InitResource(&resource, is_subresource, GURL(url), type);
    SafeBrowsingBlockingPage::ShowBlockingPage(ui_manager_.get(), resource);
  }

  // Returns the SafeBrowsingBlockingPage currently showing or NULL if none is
  // showing.
  SafeBrowsingBlockingPage* GetSafeBrowsingBlockingPage() {
    InterstitialPage* interstitial =
        InterstitialPage::GetInterstitialPage(web_contents());
    if (!interstitial)
      return NULL;
    return  static_cast<SafeBrowsingBlockingPage*>(
        interstitial->GetDelegateForTesting());
  }

  UserResponse user_response() const { return user_response_; }
  void ResetUserResponse() { user_response_ = PENDING; }

  static void ProceedThroughInterstitial(
      SafeBrowsingBlockingPage* sb_interstitial) {
    sb_interstitial->interstitial_page()->Proceed();
    // Proceed() posts a task to update the SafeBrowsingService::Client.
    base::RunLoop().RunUntilIdle();
  }

  static void DontProceedThroughInterstitial(
      SafeBrowsingBlockingPage* sb_interstitial) {
    sb_interstitial->interstitial_page()->DontProceed();
    // DontProceed() posts a task to update the SafeBrowsingService::Client.
    base::RunLoop().RunUntilIdle();
  }

  void DontProceedThroughSubresourceInterstitial(
      SafeBrowsingBlockingPage* sb_interstitial) {
    // CommandReceived(kTakeMeBackCommand) does a back navigation for
    // subresource interstitials.
    NavigationSimulator::GoBack(web_contents());
    // DontProceed() posts a task to update the SafeBrowsingService::Client.
    base::RunLoop().RunUntilIdle();
  }

  scoped_refptr<TestSafeBrowsingUIManager> ui_manager_;
  scoped_refptr<net::URLRequestContextGetter> system_request_context_getter_;

  // Owned by TestSafeBrowsingBlockingPage.
  MockTestingProfile* mock_profile_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::TestEventRouter* test_event_router_;
  std::unique_ptr<TestExtensionEventObserver> observer_;
#endif

 private:
  void InitResource(security_interstitials::UnsafeResource* resource,
                    bool is_subresource,
                    const GURL& url,
                    SBThreatType type) {
    resource->callback =
        base::Bind(&SafeBrowsingBlockingPageTest::OnBlockingPageComplete,
                   base::Unretained(this));
    resource->callback_thread = base::CreateSingleThreadTaskRunnerWithTraits(
        {content::BrowserThread::IO});
    resource->url = url;
    resource->is_subresource = is_subresource;
    resource->threat_type = type;
    resource->web_contents_getter =
        security_interstitials::UnsafeResource::GetWebContentsGetter(
            web_contents()->GetMainFrame()->GetProcess()->GetID(),
            web_contents()->GetMainFrame()->GetRoutingID());
    resource->threat_source = safe_browsing::ThreatSource::LOCAL_PVER3;
  }

  ScopedTestingLocalState scoped_testing_local_state_;
  UserResponse user_response_;
  TestSafeBrowsingBlockingPageFactory factory_;
};

class SafeBrowsingBlockingPageTestDiceEnabled
    : public SafeBrowsingBlockingPageTest {
 private:
  ScopedAccountConsistencyDice scoped_dice_;
};

// Tests showing a blocking page for a malware page and not proceeding.
TEST_F(SafeBrowsingBlockingPageTest, MalwarePageDontProceed) {
  // Enable malware details.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  SetExtendedReportingPref(profile->GetPrefs(), true);

  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());


  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Verify |OnSecurityInterstitialShown| event is triggered.
  observer_->VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialShown::kEventName, GURL(kBadURL), "MALWARE");
#endif

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, the pending entry should be gone.
  EXPECT_FALSE(controller().GetPendingEntry());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
}

// Tests showing a blocking page for a malware page and then proceeding.
TEST_F(SafeBrowsingBlockingPageTest, MalwarePageProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  SetExtendedReportingPref(profile->GetPrefs(), true);

  // Start a load.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(GURL(kBadURL),
                                                                web_contents());
  navigation->Start();

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Verify |OnSecurityInterstitialShown| event is triggered.
  observer_->VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialShown::kEventName, GURL(kBadURL), "MALWARE");
#endif

  // Simulate the user clicking "proceed".
  ProceedThroughInterstitial(sb_interstitial);

  // The interstitial is shown until the navigation commits.
  ASSERT_TRUE(InterstitialPage::GetInterstitialPage(web_contents()));
  // Commit the navigation.
  navigation->Commit();
  // The interstitial should be gone now.
  ASSERT_FALSE(InterstitialPage::GetInterstitialPage(web_contents()));

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Verify |OnSecurityInterstitialProceeded| event is triggered.
  observer_->VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialProceeded::kEventName, GURL(kBadURL), "MALWARE");
#endif
}

// Tests showing a blocking page for a page that contains malware subresources
// and not proceeding.
TEST_F(SafeBrowsingBlockingPageTest, PageWithMalwareResourceDontProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  SetExtendedReportingPref(profile->GetPrefs(), true);

  // Navigate somewhere.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL(kGoogleURL));

  // Navigate somewhere else.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL(kPageURL));

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Verify |OnSecurityInterstitialShown| event is triggered.
  observer_->VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialShown::kEventName, GURL(kPageURL), "MALWARE");
#endif

  // Simulate the user clicking "don't proceed".
  DontProceedThroughSubresourceInterstitial(sb_interstitial);
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, we should be back to the first page, the 2nd one should
  // have been removed from the navigation controller.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoogleURL, web_contents()->GetVisibleURL().spec());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
}

// Tests showing a blocking page for a page that contains malware subresources
// and proceeding.
TEST_F(SafeBrowsingBlockingPageTest, PageWithMalwareResourceProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  SetExtendedReportingPref(profile->GetPrefs(), true);

  // Navigate somewhere.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL(kPageURL));

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Verify |OnSecurityInterstitialShown| event is triggered.
  observer_->VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialShown::kEventName, GURL(kPageURL), "MALWARE");
#endif

  // Simulate the user clicking "proceed".
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did proceed, we should be back to showing the page.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kPageURL, web_contents()->GetVisibleURL().spec());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Verify |OnSecurityInterstitialProceeded| event is triggered.
  observer_->VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialProceeded::kEventName, GURL(kPageURL), "MALWARE");
#endif
}

// Tests showing a blocking page for a page that contains multiple malware
// subresources and not proceeding.  This just tests that the extra malware
// subresources (which trigger queued interstitial pages) do not break anything.
TEST_F(SafeBrowsingBlockingPageTest,
       PageWithMultipleMalwareResourceDontProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  SetExtendedReportingPref(profile->GetPrefs(), true);

  // Navigate somewhere.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL(kGoogleURL));

  // Navigate somewhere else.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL(kPageURL));

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(true, kBadURL2);
  ShowInterstitial(true, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Verify |OnSecurityInterstitialShown| event is triggered.
  observer_->VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialShown::kEventName, GURL(kPageURL), "MALWARE");
#endif

  // Simulate the user clicking "don't proceed".
  DontProceedThroughSubresourceInterstitial(sb_interstitial);
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, we should be back to the first page, the 2nd one should
  // have been removed from the navigation controller.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoogleURL, web_contents()->GetVisibleURL().spec());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
}

// Tests showing a blocking page for a page that contains multiple malware
// subresources and proceeding through the first interstitial, but not the next.
TEST_F(SafeBrowsingBlockingPageTest,
       PageWithMultipleMalwareResourceProceedThenDontProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  SetExtendedReportingPref(profile->GetPrefs(), true);

  // Navigate somewhere.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL(kGoogleURL));

  // Navigate somewhere else.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL(kPageURL));

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(true, kBadURL2);
  ShowInterstitial(true, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Verify |OnSecurityInterstitialShown| event is triggered.
  observer_->VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialShown::kEventName, GURL(kPageURL), "MALWARE");
#endif

  // Proceed through the 1st interstitial.
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Verify |OnSecurityInterstitialProceeded| event is triggered.
  observer_->VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialProceeded::kEventName, GURL(kPageURL), "MALWARE");
#endif

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();

  ResetUserResponse();

  // We should land to a 2nd interstitial (aggregating all the malware resources
  // loaded while the 1st interstitial was showing).
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Don't proceed through the 2nd interstitial.
  DontProceedThroughSubresourceInterstitial(sb_interstitial);
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, we should be back to the first page, the 2nd one should
  // have been removed from the navigation controller.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kGoogleURL, web_contents()->GetVisibleURL().spec());

  // No report should have been sent -- we don't create a report the
  // second time.
  EXPECT_EQ(0u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
}

// Tests showing a blocking page for a page that contains multiple malware
// subresources and proceeding through the multiple interstitials.
TEST_F(SafeBrowsingBlockingPageTest, PageWithMultipleMalwareResourceProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  SetExtendedReportingPref(profile->GetPrefs(), true);

  // Navigate somewhere else.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL(kPageURL));

  // Simulate that page loading a bad-resource triggering an interstitial.
  ShowInterstitial(true, kBadURL);

  // More bad resources loading causing more interstitials. The new
  // interstitials should be queued.
  ShowInterstitial(true, kBadURL2);
  ShowInterstitial(true, kBadURL3);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Verify |OnSecurityInterstitialShown| event is triggered.
  observer_->VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialShown::kEventName, GURL(kPageURL), "MALWARE");
#endif

  // Proceed through the 1st interstitial.
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());

  // A report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();

  ResetUserResponse();

  // We should land to a 2nd interstitial (aggregating all the malware resources
  // loaded while the 1st interstitial was showing).
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Proceed through the 2nd interstitial.
  ProceedThroughInterstitial(sb_interstitial);
  EXPECT_EQ(OK, user_response());

  // We did proceed, we should be back to the initial page.
  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_EQ(kPageURL, web_contents()->GetVisibleURL().spec());

  // No report should have been sent -- we don't create a report the
  // second time.
  EXPECT_EQ(0u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Verify |OnSecurityInterstitialProceeded| event is triggered.
  observer_->VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialProceeded::kEventName, GURL(kPageURL), "MALWARE");
#endif
}

// Tests showing a blocking page then navigating back and forth to make sure the
// controller entries are OK.  http://crbug.com/17627
TEST_F(SafeBrowsingBlockingPageTest, NavigatingBackAndForth) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  SetExtendedReportingPref(profile->GetPrefs(), true);

  // Navigate somewhere.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL(kPageURL));

  // Now navigate to a bad page triggerring an interstitial.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(GURL(kBadURL),
                                                                web_contents());
  navigation->Start();
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Proceed, then navigate back.
  ProceedThroughInterstitial(sb_interstitial);
  navigation->Commit();
  NavigationSimulator::GoBack(web_contents());

  // We are back on the good page.
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_FALSE(sb_interstitial);
  ASSERT_EQ(2, controller().GetEntryCount());
  EXPECT_EQ(kPageURL, web_contents()->GetVisibleURL().spec());

  // Navigate forward to the malware URL.
  auto forward_navigation = NavigationSimulator::CreateHistoryNavigation(
      1 /* Offset */, web_contents());
  forward_navigation->Start();
  ShowInterstitial(false, kBadURL);
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  // Let's proceed and make sure everything is OK (bug 17627).
  ProceedThroughInterstitial(sb_interstitial);
  // Commit the navigation.
  forward_navigation->Commit();
  sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_FALSE(sb_interstitial);
  ASSERT_EQ(2, controller().GetEntryCount());
  EXPECT_EQ(kBadURL, web_contents()->GetVisibleURL().spec());

  // Two reports should have been sent.
  EXPECT_EQ(2u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
}

// Tests that calling "don't proceed" after "proceed" has been called doesn't
// cause problems. http://crbug.com/30079
TEST_F(SafeBrowsingBlockingPageTest, ProceedThenDontProceed) {
  // Enable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  SetExtendedReportingPref(profile->GetPrefs(), true);

  // Start a load.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(GURL(kBadURL),
                                                                web_contents());
  navigation->Start();

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "proceed" then "don't proceed" (before the
  // interstitial is shown).
  sb_interstitial->interstitial_page()->Proceed();
  sb_interstitial->interstitial_page()->DontProceed();
  // Proceed() and DontProceed() post a task to update the
  // SafeBrowsingService::Client.
  base::RunLoop().RunUntilIdle();

  // The interstitial should be gone.
  EXPECT_EQ(OK, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // Only one report should have been sent.
  EXPECT_EQ(1u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
}

// Tests showing a blocking page for a malware page with reports disabled.
TEST_F(SafeBrowsingBlockingPageTest, MalwareReportsDisabled) {
  // Disable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  SetExtendedReportingPref(profile->GetPrefs(), false);

  // Start a load.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(GURL(kBadURL),
                                                                web_contents());
  navigation->Start();

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  EXPECT_TRUE(sb_interstitial->sb_error_ui()->CanShowExtendedReportingOption());

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // We did not proceed, the pending entry should be gone.
  EXPECT_FALSE(controller().GetPendingEntry());

  // No report should have been sent.
  EXPECT_EQ(0u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
}

// Test that toggling the checkbox has the anticipated effects.
TEST_F(SafeBrowsingBlockingPageTest, MalwareReportsToggling) {
  // Disable malware reports.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  SetExtendedReportingPref(profile->GetPrefs(), false);

  // Start a load.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(GURL(kBadURL),
                                                                web_contents());
  navigation->Start();

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  EXPECT_TRUE(sb_interstitial->sb_error_ui()->CanShowExtendedReportingOption());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsExtendedReportingEnabled(*profile->GetPrefs()));

  // Simulate the user check the report agreement checkbox.
  sb_interstitial->controller()->SetReportingPreference(true);

  EXPECT_TRUE(IsExtendedReportingEnabled(*profile->GetPrefs()));

  // Simulate the user uncheck the report agreement checkbox.
  sb_interstitial->controller()->SetReportingPreference(false);

  EXPECT_FALSE(IsExtendedReportingEnabled(*profile->GetPrefs()));
}

// Test that extended reporting option is not shown in incognito window.
TEST_F(SafeBrowsingBlockingPageTest,
       ExtendedReportingNotShownInIncognito) {
  // Make profile in incognito mode.
  SetProfileOffTheRecord();
  // Enable malware details.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  ASSERT_TRUE(profile->IsOffTheRecord());
  SetExtendedReportingPref(profile->GetPrefs(), true);

  // Start a load.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(GURL(kBadURL),
                                                                web_contents());
  navigation->Start();

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  EXPECT_FALSE(sb_interstitial->sb_error_ui()
                    ->CanShowExtendedReportingOption());

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // No report should have been sent.
  EXPECT_EQ(0u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
}

// Test that extended reporting option is not shown if
// kSafeBrowsingExtendedReportingOptInAllowed is disabled.
TEST_F(SafeBrowsingBlockingPageTest,
       ExtendedReportingNotShownNotAllowExtendedReporting) {
  // Enable malware details.
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);

  // Start a load.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(GURL(kBadURL),
                                                                web_contents());
  navigation->Start();

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  EXPECT_FALSE(sb_interstitial->sb_error_ui()
                   ->CanShowExtendedReportingOption());

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());

  // No report should have been sent.
  EXPECT_EQ(0u, ui_manager_->GetThreatDetails()->size());
  ui_manager_->GetThreatDetails()->clear();
}

// Tests showing a blocking page for billing.
TEST_F(SafeBrowsingBlockingPageTest, BillingPage) {
  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL, SB_THREAT_TYPE_BILLING);

  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  base::DictionaryValue load_time_data;
  sb_interstitial->sb_error_ui()->PopulateStringsForHtml(&load_time_data);

  base::string16 str;

  load_time_data.GetString("heading", &str);
  EXPECT_EQ(str, l10n_util::GetStringUTF16(IDS_BILLING_HEADING));
  load_time_data.GetString("primaryParagraph", &str);
  EXPECT_EQ(str, l10n_util::GetStringUTF16(IDS_BILLING_PRIMARY_PARAGRAPH));
  load_time_data.GetString("primaryButtonText", &str);
  EXPECT_EQ(str, l10n_util::GetStringUTF16(IDS_BILLING_PRIMARY_BUTTON));
  load_time_data.GetString("proceedButtonText", &str);
  EXPECT_EQ(str, l10n_util::GetStringUTF16(IDS_BILLING_PROCEED_BUTTON));

  load_time_data.GetString("openDetails", &str);
  EXPECT_EQ(str, base::string16());
  load_time_data.GetString("closeDetails", &str);
  EXPECT_EQ(str, base::string16());
  load_time_data.GetString("explanationParagraph", &str);
  EXPECT_EQ(str, base::string16());
  load_time_data.GetString("finalParagraph", &str);
  EXPECT_EQ(str, base::string16());

  bool flag;
  load_time_data.GetBoolean("billing", &flag);
  EXPECT_TRUE(flag);
  load_time_data.GetBoolean("phishing", &flag);
  EXPECT_FALSE(flag);
  load_time_data.GetBoolean("overridable", &flag);
  EXPECT_TRUE(flag);
  load_time_data.GetBoolean("hide_primary_button", &flag);
  EXPECT_FALSE(flag);
}

class SafeBrowsingBlockingQuietPageTest
    : public ChromeRenderViewHostTestHarness {
 public:
  // The decision the user made.
  enum UserResponse { PENDING, OK, CANCEL };

  SafeBrowsingBlockingQuietPageTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {
    // The safe browsing UI manager does not need a service for this test.
    ui_manager_ = new TestSafeBrowsingUIManager(NULL);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    system_request_context_getter_ =
        base::MakeRefCounted<net::TestURLRequestContextGetter>(
            base::CreateSingleThreadTaskRunnerWithTraits(
                {content::BrowserThread::IO}));
    TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(
        system_request_context_getter_.get());

    SafeBrowsingBlockingPage::RegisterFactory(&factory_);
    SafeBrowsingUIManager::CreateWhitelistForTesting(web_contents());

    safe_browsing::TestSafeBrowsingServiceFactory sb_service_factory;
    sb_service_factory.SetTestUIManager(ui_manager_.get());
    auto* safe_browsing_service =
        sb_service_factory.CreateSafeBrowsingService();
    // A profile was created already but SafeBrowsingService wasn't around to
    // get notified of it, so include that notification now.
    safe_browsing_service->AddPrefService(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->GetPrefs());
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        safe_browsing_service);
    g_browser_process->safe_browsing_service()->Initialize();
  }

  void TearDown() override {
    // Release the UI manager before the BrowserThreads are destroyed.
    ui_manager_ = NULL;
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    SafeBrowsingBlockingPage::RegisterFactory(NULL);
    TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(nullptr);
    system_request_context_getter_ = nullptr;
    // Clean up singleton reference (crbug.com/110594).
    ThreatDetails::RegisterFactory(NULL);

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void OnBlockingPageComplete(bool proceed) {
    if (proceed)
      user_response_ = OK;
    else
      user_response_ = CANCEL;
  }

  void ShowInterstitial(bool is_subresource,
                        const char* url,
                        SBThreatType type) {
    security_interstitials::UnsafeResource resource;
    InitResource(&resource, is_subresource, GURL(url), type);
    SafeBrowsingBlockingPage::ShowBlockingPage(ui_manager_.get(), resource);
  }

  // Returns the SafeBrowsingBlockingPage currently showing or NULL if none is
  // showing.
  TestSafeBrowsingBlockingPageQuiet* GetSafeBrowsingBlockingPage() {
    InterstitialPage* interstitial =
        InterstitialPage::GetInterstitialPage(web_contents());
    if (!interstitial)
      return NULL;
    return static_cast<TestSafeBrowsingBlockingPageQuiet*>(
        interstitial->GetDelegateForTesting());
  }

  scoped_refptr<TestSafeBrowsingUIManager> ui_manager_;

  // Owned by TestSafeBrowsingBlockingQuietPage.
  MockTestingProfile* mock_profile_;

 private:
  void InitResource(security_interstitials::UnsafeResource* resource,
                    bool is_subresource,
                    const GURL& url,
                    SBThreatType type) {
    resource->callback =
        base::Bind(&SafeBrowsingBlockingQuietPageTest::OnBlockingPageComplete,
                   base::Unretained(this));
    resource->callback_thread = base::CreateSingleThreadTaskRunnerWithTraits(
        {content::BrowserThread::IO});
    resource->url = url;
    resource->is_subresource = is_subresource;
    resource->threat_type = type;
    resource->web_contents_getter =
        security_interstitials::UnsafeResource::GetWebContentsGetter(
            web_contents()->GetMainFrame()->GetProcess()->GetID(),
            web_contents()->GetMainFrame()->GetRoutingID());
    resource->threat_source = safe_browsing::ThreatSource::LOCAL_PVER3;
  }

  ScopedTestingLocalState scoped_testing_local_state_;
  UserResponse user_response_;
  TestSafeBrowsingBlockingQuietPageFactory factory_;
  scoped_refptr<net::URLRequestContextGetter> system_request_context_getter_;
};

// Tests showing a quiet blocking page for a malware page.
TEST_F(SafeBrowsingBlockingQuietPageTest, MalwarePage) {
  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL, SB_THREAT_TYPE_URL_MALWARE);
  TestSafeBrowsingBlockingPageQuiet* sb_interstitial =
      GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  base::DictionaryValue load_time_data = sb_interstitial->GetUIStrings();
  base::string16 str;
  load_time_data.GetString("heading", &str);
  EXPECT_EQ(str, l10n_util::GetStringUTF16(IDS_MALWARE_WEBVIEW_HEADING));
  bool is_giant;
  load_time_data.GetBoolean("is_giant", &is_giant);
  EXPECT_FALSE(is_giant);
}

// Tests showing a quiet blocking page for a phishing page.
TEST_F(SafeBrowsingBlockingQuietPageTest, PhishingPage) {
  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL, SB_THREAT_TYPE_URL_PHISHING);
  TestSafeBrowsingBlockingPageQuiet* sb_interstitial =
      GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);

  base::DictionaryValue load_time_data = sb_interstitial->GetUIStrings();
  base::string16 str;
  load_time_data.GetString("heading", &str);
  EXPECT_EQ(str, l10n_util::GetStringUTF16(IDS_PHISHING_WEBVIEW_HEADING));
  bool is_giant;
  load_time_data.GetBoolean("is_giant", &is_giant);
  EXPECT_FALSE(is_giant);
}

// Tests showing a quiet blocking page in a giant webview.
TEST_F(SafeBrowsingBlockingQuietPageTest, GiantWebView) {
  // Start a load.
  controller().LoadURL(GURL(kBadURL), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL, SB_THREAT_TYPE_URL_MALWARE);
  TestSafeBrowsingBlockingPageQuiet* sb_interstitial =
      GetSafeBrowsingBlockingPage();
  EXPECT_TRUE(sb_interstitial);

  sb_interstitial->SetGiantWebView();
  base::DictionaryValue load_time_data = sb_interstitial->GetUIStrings();
  bool is_giant;
  load_time_data.GetBoolean("is_giant", &is_giant);
  EXPECT_TRUE(is_giant);
}

// Test that extended reporting option is not shown if Unified Consent is
// enabled.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#define TEST_CLASS_ExtendedReportingNotShownUnifiedConsent \
  SafeBrowsingBlockingPageTestDiceEnabled
#else
#define TEST_CLASS_ExtendedReportingNotShownUnifiedConsent \
  SafeBrowsingBlockingPageTest
#endif
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ExtendedReportingNotShownUnifiedConsent \
  DISABLED_ExtendedReportingNotShownUnifiedConsent
#else
#define MAYBE_ExtendedReportingNotShownUnifiedConsent \
  ExtendedReportingNotShownUnifiedConsent
#endif
TEST_F(TEST_CLASS_ExtendedReportingNotShownUnifiedConsent,
       MAYBE_ExtendedReportingNotShownUnifiedConsent) {
  // Enable unified consent.
  unified_consent::ScopedUnifiedConsent scoped_unified_consent(
      unified_consent::UnifiedConsentFeatureState::kEnabledWithBump);
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  // Fake sign in so unified consent can be given.
  SigninManagerFactory::GetForProfile(profile)->SetAuthenticatedAccountInfo(
      "gaia_id", "user");

  // Give unified consent.
  UnifiedConsentServiceFactory::GetForProfile(profile)->SetUnifiedConsentGiven(
      true);

  // Start a load.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(GURL(kBadURL),
                                                                web_contents());
  navigation->Start();

  // Simulate the load causing a safe browsing interstitial to be shown.
  ShowInterstitial(false, kBadURL);
  SafeBrowsingBlockingPage* sb_interstitial = GetSafeBrowsingBlockingPage();
  ASSERT_TRUE(sb_interstitial);
  EXPECT_FALSE(
      sb_interstitial->sb_error_ui()->CanShowExtendedReportingOption());

  base::RunLoop().RunUntilIdle();

  // Simulate the user clicking "don't proceed".
  DontProceedThroughInterstitial(sb_interstitial);

  // The interstitial should be gone.
  EXPECT_EQ(CANCEL, user_response());
  EXPECT_FALSE(GetSafeBrowsingBlockingPage());
}

}  // namespace safe_browsing
