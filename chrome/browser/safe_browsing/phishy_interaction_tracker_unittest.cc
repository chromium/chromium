// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/phishy_interaction_tracker.h"

#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
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
      bool is_subresource,
      content::NavigationEntry* entry,
      WebContents* web_contents,
      bool allowlist_only,
      safe_browsing::SBThreatType* threat_type) override {
    *threat_type = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
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
    ChromeRenderViewHostTestHarness::SetUp();

    feature_list_.InitAndEnableFeature(safe_browsing::kAntiPhishingTelemetry);
    ui_manager_ = new StrictMock<MockSafeBrowsingUIManager>();
    phishy_interaction_tracker_ =
        base::WrapUnique(new PhishyInteractionTracker(web_contents()));
    phishy_interaction_tracker_->SetUIManagerForTesting(ui_manager_.get());
    phishy_interaction_tracker_->HandlePageChanged();
  }

  void TearDown() override {
    // Delete the tracker object on the UI thread and release the
    // SafeBrowsingService.
    content::GetUIThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, phishy_interaction_tracker_.release());
    ui_manager_.reset();
    phishy_interaction_tracker_.reset();
    base::RunLoop().RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  security_interstitials::UnsafeResource MakeUnsafeResource(
      const char* url,
      bool is_subresource) {
    security_interstitials::UnsafeResource resource;
    resource.url = GURL(url);
    resource.is_subresource = is_subresource;
    resource.threat_type =
        safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
    return resource;
  }

  void TriggerClickEvent() {
    blink::WebMouseEvent mouse_event = blink::WebMouseEvent(
        blink::WebInputEvent::Type::kMouseDown, gfx::PointF(), gfx::PointF(),
        blink::WebPointerProperties::Button::kBack, 0, 0,
        base::TimeTicks::Now());
    phishy_interaction_tracker_->HandleInputEvent(mouse_event);
  }

  void TriggerKeyEvent() {
    content::NativeWebKeyboardEvent key_event(
        blink::WebKeyboardEvent::Type::kChar,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
    phishy_interaction_tracker_->HandleInputEvent(key_event);
  }

  void TriggerPasteEvent() { phishy_interaction_tracker_->HandlePasteEvent(); }

  void SetNullDelayForTest() {
    phishy_interaction_tracker_->SetInactivityDelayForTesting(
        base::Milliseconds(0));
  }

 protected:
  std::unique_ptr<PhishyInteractionTracker> phishy_interaction_tracker_;
  scoped_refptr<MockSafeBrowsingUIManager> ui_manager_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PhishyInteractionTrackerTest, CheckHistogramCountsOnPhishyUserEvents) {
  base::HistogramTester histogram_tester_;

  security_interstitials::UnsafeResource resource =
      MakeUnsafeResource(kBadURL, false /* is_subresource */);
  safe_browsing::SBThreatType threat_type;
  EXPECT_TRUE(ui_manager_->IsUrlAllowlistedOrPendingForWebContents(
      resource.url, resource.is_subresource, /*entry=*/nullptr,
      security_interstitials::GetWebContentsForResource(resource), true,
      &threat_type));

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
