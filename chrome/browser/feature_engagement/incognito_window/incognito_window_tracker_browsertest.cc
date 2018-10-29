// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/feature_promos/incognito_window_promo_bubble_view.h"
#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace feature_engagement {
namespace {

MATCHER_P(IsFeature, feature, "") {
  return arg.name == feature.name;
}

std::unique_ptr<KeyedService> BuildTestTrackerFactory(
    content::BrowserContext* context) {
  return std::make_unique<testing::StrictMock<test::MockTracker>>();
}

// Set up a test profile for the incognito window In-Product Help (IPH)
// tracker.
class IncognitoWindowTrackerBrowserTest : public InProcessBrowserTest {
 public:
  IncognitoWindowTrackerBrowserTest() = default;
  ~IncognitoWindowTrackerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    TrackerFactory::GetInstance()->SetTestingFactoryAndUse(
        browser()->profile(), base::BindRepeating(&BuildTestTrackerFactory));

    // Ensure all initialization is finished.
    base::RunLoop().RunUntilIdle();

    feature_engagement_tracker_ = static_cast<test::MockTracker*>(
        TrackerFactory::GetForBrowserContext(browser()->profile()));

    EXPECT_CALL(*feature_engagement_tracker_, IsInitialized())
        .WillOnce(::testing::Return(true));

    ASSERT_TRUE(TrackerFactory::GetForBrowserContext(browser()->profile())
                    ->IsInitialized());
  }

 protected:
  // Owned by the Profile.
  test::MockTracker* feature_engagement_tracker_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(IncognitoWindowTrackerBrowserTest);
};

}  // namespace

// Test that after meeting all the requirements, the incognito window
// In-Product Help (IPH) promo is visible.
IN_PROC_BROWSER_TEST_F(IncognitoWindowTrackerBrowserTest, ShowPromo) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Bypass the 2 hour active session time requirement.
  EXPECT_CALL(*feature_engagement_tracker_,
              NotifyEvent(events::kIncognitoWindowSessionTimeMet));
  auto* incognito_window_tracker =
      IncognitoWindowTrackerFactory::GetInstance()->GetForProfile(
          browser()->profile());

  incognito_window_tracker->OnSessionTimeMet();

  incognito_window_tracker
      ->UseDefaultForChromeVariationConfigurationReleaseTimeForTesting();

  // Set up feature engagement ShouldTriggerHelpUI mock.
  EXPECT_CALL(*feature_engagement_tracker_,
              ShouldTriggerHelpUI(IsFeature(kIPHIncognitoWindowFeature)))
      .WillOnce(::testing::Return(true))
      .WillRepeatedly(::testing::Return(false));

  EXPECT_CALL(*feature_engagement_tracker_,
              GetTriggerState(IsFeature(kIPHIncognitoWindowFeature)))
      .WillRepeatedly(
          ::testing::Return(Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED));

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebUI* web_ui = web_contents->GetWebUI();

  auto handler_owner =
      std::make_unique<settings::ClearBrowsingDataHandler>(web_ui);
  settings::ClearBrowsingDataHandler* handler = handler_owner.get();
  web_ui->AddMessageHandler(std::move(handler_owner));
  handler->AllowJavascriptForTesting();
  handler->HandleClearBrowsingDataForTest();

  auto* widget = incognito_window_tracker->incognito_promo()->GetWidget();

  EXPECT_TRUE(widget->IsVisible());

  // Tracker::Dismissed() must be invoked when the promo is closed. This will
  // clear the flag for whether there is any in-product help being displayed.
  EXPECT_CALL(*feature_engagement_tracker_,
              Dismissed(IsFeature(kIPHIncognitoWindowFeature)));

  widget->Close();
}

}  // namespace feature_engagement
