// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_reminder_desktop_ui_controller.h"

#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_reminder_desktop_ui_controller_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_reminder_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/privacy_sandbox/tracking_protection_reminder_service.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace privacy_sandbox {
namespace {

BrowserFeaturePromoController* GetFeaturePromoController(Browser* browser) {
  auto* promo_controller = static_cast<BrowserFeaturePromoController*>(
      browser->window()->GetFeaturePromoController());
  return promo_controller;
}

}  // namespace

using NoticeType = privacy_sandbox::TrackingProtectionOnboarding::NoticeType;
using SurfaceType = privacy_sandbox::TrackingProtectionOnboarding::SurfaceType;

class TrackingProtectionReminderDesktopUiControllerTest
    : public InteractiveFeaturePromoTest {
 protected:
  TrackingProtectionReminderDesktopUiControllerTest()
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos(GetIphFeature())) {}
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(embedded_test_server()->Start());
    InteractiveFeaturePromoTest::SetUpOnMainThread();
  }
  privacy_sandbox::TrackingProtectionReminderDesktopUiController*
  desktop_reminder_service() {
    return TrackingProtectionReminderDesktopUiControllerFactory::GetForProfile(
        browser()->profile());
  }
  privacy_sandbox::TrackingProtectionReminderService* reminder_service() {
    return TrackingProtectionReminderFactory::GetForProfile(
        browser()->profile());
  }
  privacy_sandbox::TrackingProtectionOnboarding* onboarding_service() {
    return TrackingProtectionOnboardingFactory::GetForProfile(
        browser()->profile());
  }
  void ShowOnboardingNotice(bool is_silent) {
    if (is_silent) {
      onboarding_service()->MaybeMarkModeBSilentEligible();
      onboarding_service()->NoticeShown(SurfaceType::kDesktop,
                                        NoticeType::kModeBSilentOnboarding);
    } else {
      onboarding_service()->MaybeMarkModeBEligible();
      onboarding_service()->NoticeShown(SurfaceType::kDesktop,
                                        NoticeType::kModeBOnboarding);
    }
  }
  void CallOnboardingObserver(bool is_silent) {
    if (is_silent) {
      reminder_service()->OnTrackingProtectionSilentOnboardingUpdated(
          TrackingProtectionOnboarding::SilentOnboardingStatus::kOnboarded);
    } else {
      reminder_service()->OnTrackingProtectionOnboardingUpdated(
          TrackingProtectionOnboarding::OnboardingStatus::kOnboarded);
    }
  }
  void SetIsModeBUser(bool is_mode_b_user) {
    reminder_service()->is_mode_b_user_ = is_mode_b_user;
  }
  bool IsReminderIphActive(Browser* browser) {
    return GetFeaturePromoController(browser)->IsPromoActive(
        feature_engagement::kIPHTrackingProtectionReminderFeature);
  }
  std::vector<base::test::FeatureRef> GetIphFeature() {
    return {feature_engagement::kIPHTrackingProtectionReminderFeature};
  }

  static base::Time FakeTimeNow() { return fake_time_; }
  static void SetFakeNow(base::Time fake_now) { fake_time_ = fake_now; }
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  static base::Time fake_time_;
};

base::Time TrackingProtectionReminderDesktopUiControllerTest::fake_time_;

class TrackingProtectionReminderDesktopUiControllerIphTest
    : public TrackingProtectionReminderDesktopUiControllerTest {
 protected:
  TrackingProtectionReminderDesktopUiControllerIphTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

 private:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{privacy_sandbox::kTrackingProtectionReminder,
             {{"is-silent-reminder", "false"}, {"reminder-delay", "0ms"}}}};
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TrackingProtectionReminderDesktopUiControllerIphTest,
                       ReminderIsShown) {
  base::Time current_time;
  EXPECT_TRUE(base::Time::FromString("22 Jul 2024 12:00 GMT", &current_time));
  SetFakeNow(current_time);
  base::subtle::ScopedTimeClockOverrides override(
      &TrackingProtectionReminderDesktopUiControllerTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  // Set reminder status to `kPendingReminder`.
  SetIsModeBUser(false);
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);
  EXPECT_EQ(reminder_service()->GetReminderNoticeData(
                TrackingProtectionOnboarding::SurfaceType::kDesktop),
            std::nullopt);

  // Open a new tab with the tracking protection icon visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Confirm that the reminder is showning.
  EXPECT_TRUE(IsReminderIphActive(browser()));
  // Confirm that the status was updated aftering seeing a reminder.
  EXPECT_EQ(reminder_service()->GetReminderStatus(),
            tracking_protection::TrackingProtectionReminderStatus::
                kExperiencedReminder);
  // Confirm that the timestamp was recorded.
  EXPECT_EQ(reminder_service()
                ->GetReminderNoticeData(
                    TrackingProtectionOnboarding::SurfaceType::kDesktop)
                ->notice_first_shown,
            FakeTimeNow());
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionReminderDesktopUiControllerIphTest,
                       ReminderCanceled) {
  // Set up the environment such that we will receive a reminder.
  SetIsModeBUser(false);
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);
  // Ensure that values have not yet been set.
  EXPECT_EQ(reminder_service()->GetReminderNoticeData(
                TrackingProtectionOnboarding::SurfaceType::kDesktop),
            std::nullopt);

  // Open a new tab with the tracking protection icon visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_TRUE(IsReminderIphActive(browser()));
  // Close the IPH via the cancel button.
  RunTestSequence(
      PressButton(user_education::HelpBubbleView::kCloseButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
          true));
  EXPECT_FALSE(IsReminderIphActive(browser()));

  // Confirm that the reminder close event was recorded.
  EXPECT_GT(reminder_service()
                ->GetReminderNoticeData(
                    TrackingProtectionOnboarding::SurfaceType::kDesktop)
                ->notice_shown_duration,
            base::Seconds(0));
  EXPECT_EQ(reminder_service()
                ->GetReminderNoticeData(
                    TrackingProtectionOnboarding::SurfaceType::kDesktop)
                ->notice_action_taken,
            privacy_sandbox::NoticeActionTaken::kClosed);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionReminderDesktopUiControllerIphTest,
                       ReminderIsCanceledOnTabSwitch) {
  // Set up the environment such that we will receive a reminder.
  SetIsModeBUser(false);
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);
  // Ensure that values have not yet been set.
  EXPECT_EQ(reminder_service()->GetReminderNoticeData(
                TrackingProtectionOnboarding::SurfaceType::kDesktop),
            std::nullopt);

  // Open a new tab with the tracking protection icon visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Creates new background tab to switch to.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("b.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_TRUE(IsReminderIphActive(browser()));
  // Switch to the next tab.
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_FALSE(IsReminderIphActive(browser()));

  // Confirm that the reminder close event was recorded.
  EXPECT_GT(reminder_service()
                ->GetReminderNoticeData(
                    TrackingProtectionOnboarding::SurfaceType::kDesktop)
                ->notice_shown_duration,
            base::Seconds(0));
  EXPECT_EQ(reminder_service()
                ->GetReminderNoticeData(
                    TrackingProtectionOnboarding::SurfaceType::kDesktop)
                ->notice_action_taken,
            privacy_sandbox::NoticeActionTaken::kClosed);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionReminderDesktopUiControllerIphTest,
                       ReminderTimesOut) {
  // Set up the environment such that we will receive a reminder.
  SetIsModeBUser(false);
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);
  // Ensure that values have not yet been set.
  EXPECT_EQ(reminder_service()->GetReminderNoticeData(
                TrackingProtectionOnboarding::SurfaceType::kDesktop),
            std::nullopt);

  // Open a new tab with the tracking protection icon visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_TRUE(IsReminderIphActive(browser()));
  // Wait for the IPH to timeout.
  RunTestSequence(WaitForHide(
      user_education::HelpBubbleView::kHelpBubbleElementIdForTesting, true));
  EXPECT_FALSE(IsReminderIphActive(browser()));

  // Confirm that the reminder close event was recorded.
  EXPECT_GT(reminder_service()
                ->GetReminderNoticeData(
                    TrackingProtectionOnboarding::SurfaceType::kDesktop)
                ->notice_shown_duration,
            base::Seconds(0));
  EXPECT_EQ(reminder_service()
                ->GetReminderNoticeData(
                    TrackingProtectionOnboarding::SurfaceType::kDesktop)
                ->notice_action_taken,
            privacy_sandbox::NoticeActionTaken::kTimedOut);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionReminderDesktopUiControllerIphTest,
                       ReminderIsNotShown) {
  // Update the profile such that they are eligible to see a reminder.
  SetIsModeBUser(false);
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);

  // Open a new tab with the tracking protection icon not visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/blank.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Confirm that the reminder is not showning.
  EXPECT_FALSE(IsReminderIphActive(browser()));
  // Confirm the reminder status did not change.
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionReminderDesktopUiControllerIphTest,
                       UserNotOnboarded) {
  // Update the profile such that they are eligible to see a reminder.
  SetIsModeBUser(false);
  // Omit showing the onboarding notice.
  CallOnboardingObserver(/*is_silent=*/false);
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);

  // Open a new tab with the tracking protection icon not visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Confirm that the reminder is not showning.
  EXPECT_FALSE(IsReminderIphActive(browser()));
  // Confirm the reminder status did not change.
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);
}

class TrackingProtectionReminderDesktopUiControllerSilentReminderIphTest
    : public TrackingProtectionReminderDesktopUiControllerTest,
      public testing::WithParamInterface<bool> {
 protected:
  TrackingProtectionReminderDesktopUiControllerSilentReminderIphTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }
  void EnableTrackingProtection() {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kTrackingProtection3pcdEnabled, true);
  }

 private:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{privacy_sandbox::kTrackingProtectionReminder,
             {{"is-silent-reminder", "true"}, {"reminder-delay", "0ms"}}}};
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    TrackingProtectionReminderDesktopUiControllerSilentReminderIphTest,
    SilentReminderExperienced) {
  SetFakeNow(base::Time::Now());
  base::subtle::ScopedTimeClockOverrides override(
      &TrackingProtectionReminderDesktopUiControllerTest::FakeTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
  // Update the profile such that they are eligible to experience a silent
  // reminder.
  SetIsModeBUser(false);
  ShowOnboardingNotice(/*is_silent=*/GetParam());
  CallOnboardingObserver(/*is_silent=*/GetParam());
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);
  EXPECT_EQ(reminder_service()->GetReminderNoticeData(
                TrackingProtectionOnboarding::SurfaceType::kDesktop),
            std::nullopt);

  // Force the User bypass icon to be visible.
  EnableTrackingProtection();

  // Open a new tab with the tracking protection icon visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Confirm that the reminder is not showning.
  EXPECT_FALSE(IsReminderIphActive(browser()));

  // Confirm the reminder status was updated after experiencing a silent
  // reminder.
  EXPECT_EQ(reminder_service()->GetReminderStatus(),
            tracking_protection::TrackingProtectionReminderStatus::
                kExperiencedReminder);
  // Confirm that the experienced timestamp was logged.
  EXPECT_EQ(reminder_service()
                ->GetReminderNoticeData(
                    TrackingProtectionOnboarding::SurfaceType::kDesktop)
                ->notice_first_shown,
            FakeTimeNow());
}
IN_PROC_BROWSER_TEST_P(
    TrackingProtectionReminderDesktopUiControllerSilentReminderIphTest,
    UserNotOnboarded) {
  // Update the profile such that they are eligible to experience a silent
  // reminder.
  SetIsModeBUser(false);
  // Omit onboarding the user.
  CallOnboardingObserver(/*is_silent=*/GetParam());
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);

  // Open a new tab with the tracking protection icon visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Confirm that the reminder is not showning.
  EXPECT_FALSE(IsReminderIphActive(browser()));

  // Confirm the reminder status was not updated since the silent reminder was
  // not experienced.
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);
}
INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionReminderDesktopUiControllerSilentReminderIphTest,
    TrackingProtectionReminderDesktopUiControllerSilentReminderIphTest,
    /*is_silently_onboarded=*/testing::Bool());

class TrackingProtectionReminderDesktopUiControllerIphWithReminderDelayTest
    : public TrackingProtectionReminderDesktopUiControllerTest {
 protected:
  TrackingProtectionReminderDesktopUiControllerIphWithReminderDelayTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

 private:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{privacy_sandbox::kTrackingProtectionReminder,
             {{"is-silent-reminder", "false"}, {"reminder-delay", "7d"}}}};
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    TrackingProtectionReminderDesktopUiControllerIphWithReminderDelayTest,
    ReminderNotShown) {
  // Set up the profile such that they are eligible to see a reminder.
  SetIsModeBUser(false);
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);

  // Open a new tab with the tracking protection icon visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Confirm that the reminder is not showning due to a pending delay.
  EXPECT_FALSE(IsReminderIphActive(browser()));

  // Confirm the reminder status was not updated since reminder was not shown.
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);
}

class
    TrackingProtectionReminderDesktopUiControllerSilentReminderIphWithDelayTest
    : public TrackingProtectionReminderDesktopUiControllerTest,
      public testing::WithParamInterface<bool> {
 protected:
  TrackingProtectionReminderDesktopUiControllerSilentReminderIphWithDelayTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

 private:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{privacy_sandbox::kTrackingProtectionReminder,
             {{"is-silent-reminder", "true"}, {"reminder-delay", "7d"}}}};
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    TrackingProtectionReminderDesktopUiControllerSilentReminderIphWithDelayTest,
    SilentReminderExperienced) {
  // Update the profile such that they are eligible to experience a silent
  // reminder.
  SetIsModeBUser(false);
  ShowOnboardingNotice(/*is_silent=*/GetParam());
  CallOnboardingObserver(/*is_silent=*/GetParam());
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);

  // Open a new tab with the tracking protection icon visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Confirm that the reminder is not showning.
  EXPECT_FALSE(IsReminderIphActive(browser()));

  // Confirm the reminder status was not updated since a silent reminder was
  // not experienced due to delay requirement not being met.
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);
}
INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionReminderDesktopUiControllerSilentReminderIphWithDelayTest,
    TrackingProtectionReminderDesktopUiControllerSilentReminderIphWithDelayTest,
    /*is_silently_onboarded=*/testing::Bool());

class TrackingProtectionReminderDesktopUiControllerModeBUserTest
    : public TrackingProtectionReminderDesktopUiControllerTest {
 protected:
  TrackingProtectionReminderDesktopUiControllerModeBUserTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

 private:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{privacy_sandbox::kTrackingProtectionReminder,
             {{"is-silent-reminder", "false"}, {"reminder-delay", "0ms"}}}};
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    TrackingProtectionReminderDesktopUiControllerModeBUserTest,
    ReminderNotExperienced) {
  SetIsModeBUser(true);
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kModeBUserSkipped);

  // Open a new tab with the tracking protection icon visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Confirm that the reminder is not showning.
  EXPECT_FALSE(IsReminderIphActive(browser()));
  // Confirm that the reminder status was not updated.
  EXPECT_EQ(
      reminder_service()->GetReminderStatus(),
      tracking_protection::TrackingProtectionReminderStatus::kModeBUserSkipped);
}

class TrackingProtectionReminderDesktopUiControllerReminderFeatureDisabledTest
    : public TrackingProtectionReminderDesktopUiControllerTest {
 protected:
  TrackingProtectionReminderDesktopUiControllerReminderFeatureDisabledTest() {
    feature_list_.InitWithFeaturesAndParameters({}, {});
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    TrackingProtectionReminderDesktopUiControllerReminderFeatureDisabledTest,
    ReminderNotExperienced) {
  SetIsModeBUser(false);
  ShowOnboardingNotice(/*is_silent=*/false);
  CallOnboardingObserver(/*is_silent=*/false);
  EXPECT_EQ(reminder_service()->GetReminderStatus(),
            tracking_protection::TrackingProtectionReminderStatus::
                kFeatureDisabledSkipped);

  // Open a new tab with the tracking protection icon visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Confirm that the reminder is not showning.
  EXPECT_FALSE(IsReminderIphActive(browser()));
  // Confirm that the reminder status was not updated.
  EXPECT_EQ(reminder_service()->GetReminderStatus(),
            tracking_protection::TrackingProtectionReminderStatus::
                kFeatureDisabledSkipped);
}

class TrackingProtectionReminderDesktopUiControllerInvalidStatusTest
    : public TrackingProtectionReminderDesktopUiControllerTest {
 protected:
  TrackingProtectionReminderDesktopUiControllerInvalidStatusTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }

 private:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{privacy_sandbox::kTrackingProtectionReminder,
             {{"is-silent-reminder", "false"}, {"reminder-delay", "0ms"}}}};
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    TrackingProtectionReminderDesktopUiControllerInvalidStatusTest,
    ReminderNotExperienced) {
  SetIsModeBUser(false);
  ShowOnboardingNotice(/*is_silent=*/true);
  CallOnboardingObserver(/*is_silent=*/true);
  EXPECT_EQ(reminder_service()->GetReminderStatus(),
            tracking_protection::TrackingProtectionReminderStatus::kInvalid);

  // Open a new tab with the tracking protection icon visible.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(),
      https_server_.GetURL("a.test", "/third_party_partitioned_cookies.html"),
      1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Confirm that the reminder is not showning.
  EXPECT_FALSE(IsReminderIphActive(browser()));
  // Confirm that the reminder status was not updated.
  EXPECT_EQ(reminder_service()->GetReminderStatus(),
            tracking_protection::TrackingProtectionReminderStatus::kInvalid);
}

}  // namespace privacy_sandbox
