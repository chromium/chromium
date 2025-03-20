// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_PLATFORM_TEST_HELPER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_PLATFORM_TEST_HELPER_H_

#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#endif

using content::WebContents;
using security_interstitials::BaseSafeBrowsingErrorUI;

namespace safe_browsing {

class SafeBrowsingUIManager;

// A SafeBrowsingBlockingPage class that lets us wait until it's hidden.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(
      BaseUIManager* manager,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResourceList& unsafe_resources,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
      bool should_trigger_reporting,
      bool is_proceed_anyway_disabled,
      bool is_safe_browsing_surveys_enabled,
      base::OnceCallback<void(bool, SBThreatType)>
          trust_safety_sentiment_service_trigger,
      base::OnceCallback<void(bool, SBThreatType)>
          ignore_auto_revocation_notifications_trigger,
      std::optional<base::TimeTicks> blocked_page_shown_timestamp);

  // SecurityInterstitialPage methods:
  void CommandReceived(const std::string& command) override;
};

class SafeBrowsingBlockingPageTestHelper {
 public:
  SafeBrowsingBlockingPageTestHelper() = default;
  SafeBrowsingBlockingPageTestHelper(
      const SafeBrowsingBlockingPageTestHelper&) = delete;
  SafeBrowsingBlockingPageTestHelper& operator=(
      const SafeBrowsingBlockingPageTestHelper&) = delete;

  static void MaybeWaitForAsyncChecksToComplete(
      content::WebContents* web_contents,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      bool wait_for_load_stop);
};

class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory();
  ~TestSafeBrowsingBlockingPageFactory() override = default;

  enum class InterstitialShown {
    kNone,
    kConsumer,
    kEnterpriseWarn,
    kEnterpriseBlock
  };

  void SetAlwaysShowBackToSafety(bool value);
#if BUILDFLAG(FULL_SAFE_BROWSING)
  MockTrustSafetySentimentService* GetMockSentimentService();
#endif
  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* delegate,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
      bool should_trigger_reporting,
      std::optional<base::TimeTicks> blocked_page_shown_timestamp) override;
  security_interstitials::SecurityInterstitialPage* CreateEnterpriseWarnPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override;
  security_interstitials::SecurityInterstitialPage* CreateEnterpriseBlockPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources)
      override;
  InterstitialShown GetShownInterstitial();

 private:
#if BUILDFLAG(FULL_SAFE_BROWSING)
  raw_ptr<MockTrustSafetySentimentService, DanglingUntriaged>
      mock_sentiment_service_;
#endif
  bool always_show_back_to_safety_;
  InterstitialShown shown_interstitial_ = InterstitialShown::kNone;
};

// A SafeBrowsingUIManager class that allows intercepting malware details.
class FakeSafeBrowsingUIManager : public TestSafeBrowsingUIManager {
 public:
  explicit FakeSafeBrowsingUIManager(
      std::unique_ptr<SafeBrowsingBlockingPageFactory> blocking_page_factory);

  FakeSafeBrowsingUIManager(const FakeSafeBrowsingUIManager&) = delete;
  FakeSafeBrowsingUIManager& operator=(const FakeSafeBrowsingUIManager&) =
      delete;

  MOCK_METHOD0(OnAttachThreatDetailsAndLaunchSurvey, void());

  // Overrides SafeBrowsingUIManager.
  void AttachThreatDetailsAndLaunchSurvey(
      content::BrowserContext* browser_context,
      std::unique_ptr<ClientSafeBrowsingReportRequest> report) override;
  void ValidateReportForHats(std::string report_string);
  // Overrides SafeBrowsingUIManager
  void SendThreatDetails(
      content::BrowserContext* browser_context,
      std::unique_ptr<ClientSafeBrowsingReportRequest> report) override;
  void OnThreatDetailsDone(const std::string& serialized);
  void MaybeReportSafeBrowsingHit(std::unique_ptr<HitReport> hit_report,
                                  WebContents* web_contents) override;
  void MaybeSendClientSafeBrowsingWarningShownReport(
      std::unique_ptr<ClientSafeBrowsingReportRequest> report,
      WebContents* web_contents) override;
  bool hit_report_sent();
  int hit_report_count();
  bool report_sent();
  std::optional<ThreatSource> hit_report_sent_threat_source();
  std::optional<bool> report_sent_is_async_check();
  void set_threat_details_done_callback(base::OnceClosure callback);
  std::string GetReport();
  void SetExpectEmptyReportForHats(bool expect_empty_report_for_hats);
  void SetExpectReportUrlForHats(bool expect_report_url_for_hats);
  void SetExpectInterstitialInteractions(bool expect_interstitial_interactions);

 protected:
  ~FakeSafeBrowsingUIManager() override;

 private:
  std::string report_;
  base::OnceClosure threat_details_done_callback_;
  bool threat_details_done_ = false;
  int hit_report_count_ = 0;
  bool report_sent_ = false;
  bool expect_empty_report_for_hats_ = true;
  bool expect_report_url_for_hats_ = false;
  bool expect_interstitial_interactions_ = false;
  std::optional<ThreatSource> hit_report_sent_threat_source_;
  std::optional<bool> report_sent_is_async_check_;
};

class SafeBrowsingBlockingPagePlatformBrowserTest : public PlatformBrowserTest {
 public:
  SafeBrowsingBlockingPagePlatformBrowserTest() = default;

  content::WebContents* web_contents();
  bool NavigateToURL(const GURL& url);
  Profile* profile();
};

// Tests for real time URL check. To test it without making network requests to
// Safe Browsing servers, store an unsafe verdict in cache for the URL.
class SafeBrowsingBlockingPageRealTimeUrlCheckTest
    : public SafeBrowsingBlockingPagePlatformBrowserTest {
 public:
  SafeBrowsingBlockingPageRealTimeUrlCheckTest();

  SafeBrowsingBlockingPageRealTimeUrlCheckTest(
      const SafeBrowsingBlockingPageRealTimeUrlCheckTest&) = delete;
  SafeBrowsingBlockingPageRealTimeUrlCheckTest& operator=(
      const SafeBrowsingBlockingPageRealTimeUrlCheckTest&) = delete;

  void SetUp() override;
  void SetUpOnMainThread() override;
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;

 protected:
  void SetupUrlRealTimeVerdictInCacheManager(
      GURL url,
      Profile* profile,
      RTLookupResponse::ThreatInfo::VerdictType verdict_type,
      std::optional<RTLookupResponse::ThreatInfo::ThreatType> threat_type);
  void SetupUnsafeVerdict(GURL url, Profile* profile);
  void NavigateToURL(GURL url, bool expect_success = true);
  void SetReportSentCallback(base::OnceClosure callback);

  TestSafeBrowsingServiceFactory factory_;
  raw_ptr<TestSafeBrowsingBlockingPageFactory> blocking_page_factory_ptr_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_PLATFORM_TEST_HELPER_H_
