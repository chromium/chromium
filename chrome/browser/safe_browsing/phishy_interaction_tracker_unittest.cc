// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/phishy_interaction_tracker.h"

#include <string>
#include <utility>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::WebContents;

using safe_browsing::PhishyInteractionTracker;

using testing::_;
using testing::Mock;
using testing::StrictMock;

namespace {

constexpr char kBadURL[] = "https://www.phishing.com";

class MockSafeBrowsingUIManager : public safe_browsing::SafeBrowsingUIManager {
 public:
  MockSafeBrowsingUIManager()
      : SafeBrowsingUIManager(
            std::make_unique<
                safe_browsing::ChromeSafeBrowsingUIManagerDelegate>(),
            std::make_unique<
                safe_browsing::ChromeSafeBrowsingBlockingPageFactory>(),
            GURL(chrome::kChromeUINewTabURL)) {}

  MockSafeBrowsingUIManager(const MockSafeBrowsingUIManager&) = delete;
  MockSafeBrowsingUIManager& operator=(const MockSafeBrowsingUIManager&) =
      delete;

  MOCK_METHOD1(DisplayBlockingPage, void(const UnsafeResource& resource));

  bool IsUrlAllowlistedOrPendingForWebContents(
      const GURL& url,
      content::NavigationEntry* entry,
      WebContents* web_contents,
      bool allowlist_only,
      safe_browsing::SBThreatType* threat_type) override {
    *threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
    return true;
  }

 protected:
  ~MockSafeBrowsingUIManager() override {}
};

}  // namespace

class PhishyInteractionTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;

  PhishyInteractionTrackerTest() = default;
  ~PhishyInteractionTrackerTest() override = default;

  void SetUp() override {
    browser_process_ = TestingBrowserProcess::GetGlobal();
    sb_service_ =
        base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
    sb_service_->SetUseTestUrlLoaderFactory(true);
    // Set sb_service before the ChromeRenderViewHostTestHarness::SetUp(),
    // because it is needed to construct ping manager.
    browser_process_->SetSafeBrowsingService(sb_service_.get());

    ChromeRenderViewHostTestHarness::SetUp();

    ui_manager_ = new StrictMock<MockSafeBrowsingUIManager>();
    phishy_interaction_tracker_ =
        base::WrapUnique(new PhishyInteractionTracker(web_contents()));
    phishy_interaction_tracker_->SetUIManagerForTesting(ui_manager_.get());
    phishy_interaction_tracker_->HandlePageChanged();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Local state is needed to construct ProxyConfigService, which is a
    // dependency of PingManager on ChromeOS.
    TestingBrowserProcess::GetGlobal()->SetLocalState(profile()->GetPrefs());
#endif
  }

  void TearDown() override {
    browser_process_->SetSafeBrowsingService(nullptr);
    // Delete the tracker object on the UI thread and release the
    // SafeBrowsingService.
    sb_service_.reset();
    content::GetUIThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, phishy_interaction_tracker_.release());
    ui_manager_.reset();
    phishy_interaction_tracker_.reset();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
#endif
    base::RunLoop().RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  Profile* profile() {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

  security_interstitials::UnsafeResource MakeUnsafeResource(const char* url) {
    security_interstitials::UnsafeResource resource;
    resource.url = GURL(url);
    resource.threat_type =
        safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
    return resource;
  }

  std::unique_ptr<safe_browsing::ClientSafeBrowsingReportRequest>
  GetActualRequest(const network::ResourceRequest& request) {
    std::string request_string = GetUploadData(request);
    auto actual_request =
        std::make_unique<safe_browsing::ClientSafeBrowsingReportRequest>();
    actual_request->ParseFromString(request_string);
    return actual_request;
  }

  void TriggerClickEvent() {
    blink::WebMouseEvent mouse_event = blink::WebMouseEvent(
        blink::WebInputEvent::Type::kMouseDown, gfx::PointF(), gfx::PointF(),
        blink::WebPointerProperties::Button::kBack, 0, 0,
        base::TimeTicks::Now());
    phishy_interaction_tracker_->HandleInputEvent(mouse_event);
  }

  void TriggerKeyEvent() {
    input::NativeWebKeyboardEvent key_event(
        blink::WebKeyboardEvent::Type::kChar,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
    phishy_interaction_tracker_->HandleInputEvent(key_event);
  }

  void TriggerPasteEvent() { phishy_interaction_tracker_->HandlePasteEvent(); }

  void SetNullDelayForTest() {
    phishy_interaction_tracker_->SetInactivityDelayForTesting(
        base::Milliseconds(0));
  }

  void VerifyInteraction(
      const safe_browsing::ClientSafeBrowsingReportRequest& report,
      const safe_browsing::ClientSafeBrowsingReportRequest::
          PhishySiteInteraction::PhishySiteInteractionType&
              expected_interaction_type,
      const int& expected_occurrence_count) {
    // Find the interaction within the report by comparing
    // security_interstitial_interaction.
    for (auto interaction : report.phishy_site_interactions()) {
      if (interaction.phishy_site_interaction_type() ==
          expected_interaction_type) {
        EXPECT_EQ(interaction.occurrence_count(), expected_occurrence_count);
        if (expected_occurrence_count == 1) {
          EXPECT_EQ(interaction.first_interaction_timestamp_msec(),
                    interaction.last_interaction_timestamp_msec());
        } else {
          EXPECT_LE(interaction.first_interaction_timestamp_msec(),
                    interaction.last_interaction_timestamp_msec());
        }
        break;
      }
    }
  }

  void VerifyPhishyInteractionReport(
      const safe_browsing::ClientSafeBrowsingReportRequest& report,
      int expected_click_occurrences,
      int expected_key_occurrences,
      int expected_paste_occurrences) {
    ASSERT_EQ(report.type(), safe_browsing::ClientSafeBrowsingReportRequest::
                                 PHISHY_SITE_INTERACTIONS);
    ASSERT_EQ(report.phishy_site_interactions().size(), 3);
    VerifyInteraction(report,
                      safe_browsing::ClientSafeBrowsingReportRequest::
                          PhishySiteInteraction::PHISHY_CLICK_EVENT,
                      expected_click_occurrences);
    VerifyInteraction(report,
                      safe_browsing::ClientSafeBrowsingReportRequest::
                          PhishySiteInteraction::PHISHY_KEY_EVENT,
                      expected_key_occurrences);
    VerifyInteraction(report,
                      safe_browsing::ClientSafeBrowsingReportRequest::
                          PhishySiteInteraction::PHISHY_PASTE_EVENT,
                      expected_paste_occurrences);
  }

 protected:
  raw_ptr<TestingBrowserProcess> browser_process_;
  scoped_refptr<safe_browsing::TestSafeBrowsingService> sb_service_;
  std::unique_ptr<PhishyInteractionTracker> phishy_interaction_tracker_;
  scoped_refptr<MockSafeBrowsingUIManager> ui_manager_;
  safe_browsing::ChromePingManagerAllowerForTesting allow_ping_manager_;
};

TEST_F(PhishyInteractionTrackerTest, CheckHistogramCountsOnPhishyUserEvents) {
  base::HistogramTester histogram_tester_;

  security_interstitials::UnsafeResource resource = MakeUnsafeResource(kBadURL);
  safe_browsing::SBThreatType threat_type;
  EXPECT_TRUE(ui_manager_->IsUrlAllowlistedOrPendingForWebContents(
      resource.url, /*entry=*/nullptr,
      safe_browsing::unsafe_resource_util::GetWebContentsForResource(resource),
      true, &threat_type));

  const std::string phishy_interaction_histogram = "SafeBrowsing.PhishySite.";
  const int kExpectedClickEventCount = 3;
  const int kExpectedKeyEventCount = 5;
  const int kExpectedPasteEventCount = 2;
  // Trigger kExpectedClickEventCount mouse events.
  for (int i = 0; i < kExpectedClickEventCount; ++i) {
    TriggerClickEvent();
  }
  // Trigger kExpectedKeyEventCount key events.
  for (int i = 0; i < kExpectedKeyEventCount; ++i) {
    TriggerKeyEvent();
  }
  // Trigger kExpectedPasteEventCount - 1 paste events so we can trigger a
  // paste below.
  for (int i = 0; i < kExpectedPasteEventCount - 1; ++i) {
    TriggerPasteEvent();
  }
  // Set a null delay so that histograms get logged after this last user event.
  SetNullDelayForTest();
  TriggerPasteEvent();

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      phishy_interaction_histogram + "ClickEventCount",
      kExpectedClickEventCount, 1);
  histogram_tester_.ExpectUniqueSample(
      phishy_interaction_histogram + "KeyEventCount", kExpectedKeyEventCount,
      1);
  histogram_tester_.ExpectUniqueSample(
      phishy_interaction_histogram + "PasteEventCount",
      kExpectedPasteEventCount, 1);
}

TEST_F(PhishyInteractionTrackerTest, CheckPhishyUserInteractionClientReport) {
  safe_browsing::SetExtendedReportingPrefForTests(profile()->GetPrefs(), true);
  const int kExpectedClickEventCount = 4;
  const int kExpectedKeyEventCount = 1;
  const int kExpectedPasteEventCount = 3;
  auto* ping_manager =
      safe_browsing::ChromePingManagerFactory::GetForBrowserContext(profile());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::unique_ptr<safe_browsing::ClientSafeBrowsingReportRequest>
            actual_request = GetActualRequest(request);
        VerifyPhishyInteractionReport(
            *actual_request.get(), kExpectedClickEventCount,
            kExpectedKeyEventCount, kExpectedPasteEventCount);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  // Trigger kExpectedClickEventCount mouse events.
  for (int i = 0; i < kExpectedClickEventCount; ++i) {
    TriggerClickEvent();
  }
  // Trigger kExpectedKeyEventCount key events.
  for (int i = 0; i < kExpectedKeyEventCount; ++i) {
    TriggerKeyEvent();
  }
  // Trigger kExpectedPasteEventCount - 1 paste events so we can trigger a
  // paste below.
  for (int i = 0; i < kExpectedPasteEventCount - 1; ++i) {
    TriggerPasteEvent();
  }
  // Set a null delay so that histograms get logged after this last user event.
  SetNullDelayForTest();
  TriggerPasteEvent();

  base::RunLoop().RunUntilIdle();
}
