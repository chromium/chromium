// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_notification_observer.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/default_browser/default_browser_changed_notification_handler.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/default_browser/test_support/fake_shell_delegate.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/scoped_os_info_override_win.h"
#endif

namespace default_browser {

namespace {

constexpr char kInteractionHistogram[] =
    "DefaultBrowser.ChangeDetectedNotification.ShellIntegration.Interaction";
constexpr char kTimeUntilDefaultWithoutClickHistogram[] =
    "DefaultBrowser.ChangeDetectedNotification.TimeUntilDefaultWithoutClick";

}  // namespace

class DefaultBrowserNotificationObserverTest : public testing::Test {
 protected:
  DefaultBrowserNotificationObserverTest() {
    feature_list_.InitAndEnableFeature(kDefaultBrowserChangedOsNotification);
  }

  void SetUp() override {
    global_feature_override_ =
        GlobalFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindLambdaForTesting([&](BrowserProcess& browser_process) {
              auto fake_delegate = std::make_unique<FakeShellDelegate>();
              fake_delegate->set_default_state(shell_integration::IS_DEFAULT);
              return std::make_unique<DefaultBrowserManager>(
                  TestingBrowserProcess::GetGlobal(), std::move(fake_delegate),
                  base::BindLambdaForTesting(
                      [&]() -> Profile* { return profile_.get(); }));
            }));

    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/false);

    profile_ = std::make_unique<TestingProfile>();
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());
  }

  void TearDown() override {
    display_service_tester_.reset();
    profile_.reset();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  DefaultBrowserManager& manager() {
    return *DefaultBrowserManager::From(TestingBrowserProcess::GetGlobal());
  }

  NotificationDisplayServiceTester& display_service_tester() {
    return *display_service_tester_;
  }

  void ResetDisplayServiceTester() { display_service_tester_.reset(); }

  void DestroyProfile() {
    display_service_tester_.reset();
    profile_.reset();
  }

  // Helper method to create the observer and register its state-change callback
  // in `state_change_callbacks_` so tests can simulate broadcasts directly.
  std::unique_ptr<DefaultBrowserNotificationObserver> CreateObserver(
      DefaultBrowserState initial_state = shell_integration::IS_DEFAULT) {
    return std::make_unique<DefaultBrowserNotificationObserver>(
        /*register_callback=*/base::BindLambdaForTesting(
            [&](base::RepeatingCallback<void(DefaultBrowserState)> cb) {
              return state_change_callbacks_.Add(std::move(cb));
            }),
        /*initial_state_check_callback=*/
        base::BindLambdaForTesting(
            [initial_state](base::OnceCallback<void(DefaultBrowserState)> cb) {
              // Simulate the asynchronous initial state check returning.
              std::move(cb).Run(initial_state);
            }),
        manager());
  }

  void TriggerStateChange(DefaultBrowserState new_state) {
    ASSERT_FALSE(state_change_callbacks_.empty())
        << "Observer not initialized yet.";
    state_change_callbacks_.Notify(new_state);
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
#if BUILDFLAG(IS_WIN)
  base::test::ScopedOSInfoOverride win11_os_{
      base::test::ScopedOSInfoOverride::Type::kWin11Pro};
#endif
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;

  ui::UserDataFactory::ScopedOverride global_feature_override_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

  // Stores callbacks bound to the observer's `OnDefaultBrowserStateChanged()`.
  base::RepeatingCallbackList<void(DefaultBrowserState)>
      state_change_callbacks_;
};

TEST_F(DefaultBrowserNotificationObserverTest,
       ShowsNotificationWhenNotDefault) {
  auto observer = CreateObserver(shell_integration::IS_DEFAULT);

  TriggerStateChange(shell_integration::NOT_DEFAULT);

  auto notification = display_service_tester().GetNotification(
      DefaultBrowserManager::kNotificationId);

  ASSERT_TRUE(notification.has_value())
      << "Notification should be shown after state changed to NOT_DEFAULT";

  EXPECT_EQ(notification->title(),
            l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_TITLE));
  EXPECT_EQ(notification->message(),
            l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_MESSAGE));

  ASSERT_EQ(notification->buttons().size(), 2u);
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_YES_BUTTON));
  EXPECT_EQ(
      notification->buttons()[1].title,
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_NO_THANKS_BUTTON));
}

TEST_F(DefaultBrowserNotificationObserverTest,
       DoesNotShowNotificationWhenDefault) {
  auto observer = CreateObserver(shell_integration::IS_DEFAULT);

  TriggerStateChange(shell_integration::IS_DEFAULT);

  auto notification = display_service_tester().GetNotification(
      DefaultBrowserManager::kNotificationId);
  EXPECT_FALSE(notification.has_value());
}

TEST_F(DefaultBrowserNotificationObserverTest,
       DoesNotShowNotificationWhenUnknown) {
  auto observer = CreateObserver(shell_integration::IS_DEFAULT);

  TriggerStateChange(shell_integration::OTHER_MODE_IS_DEFAULT);

  auto notification = display_service_tester().GetNotification(
      DefaultBrowserManager::kNotificationId);
  EXPECT_FALSE(notification.has_value());
}

TEST_F(DefaultBrowserNotificationObserverTest,
       DoesNotShowNotificationIfAlreadyNotDefault) {
  auto observer = CreateObserver(shell_integration::NOT_DEFAULT);

  TriggerStateChange(shell_integration::NOT_DEFAULT);

  auto notification = display_service_tester().GetNotification(
      DefaultBrowserManager::kNotificationId);
  EXPECT_FALSE(notification.has_value());
}

TEST_F(DefaultBrowserNotificationObserverTest,
       IgnoresUnknownStateWithoutDroppingSubsequentLoss) {
  auto observer = CreateObserver(shell_integration::IS_DEFAULT);

  TriggerStateChange(shell_integration::UNKNOWN_DEFAULT);
  EXPECT_FALSE(display_service_tester()
                   .GetNotification(DefaultBrowserManager::kNotificationId)
                   .has_value());

  TriggerStateChange(shell_integration::NOT_DEFAULT);
  EXPECT_TRUE(display_service_tester()
                  .GetNotification(DefaultBrowserManager::kNotificationId)
                  .has_value());
}

TEST_F(DefaultBrowserNotificationObserverTest,
       AutoClosesNotificationAndRecordsMetricWhenBecomingDefaultAgain) {
  base::HistogramTester histogram_tester;
  auto observer = CreateObserver(shell_integration::IS_DEFAULT);

  TriggerStateChange(shell_integration::NOT_DEFAULT);
  ASSERT_TRUE(display_service_tester()
                  .GetNotification(DefaultBrowserManager::kNotificationId)
                  .has_value());

  task_environment().FastForwardBy(base::Milliseconds(750));
  TriggerStateChange(shell_integration::UNKNOWN_DEFAULT);
  EXPECT_TRUE(display_service_tester()
                  .GetNotification(DefaultBrowserManager::kNotificationId)
                  .has_value());

  TriggerStateChange(shell_integration::IS_DEFAULT);
  EXPECT_FALSE(display_service_tester()
                   .GetNotification(DefaultBrowserManager::kNotificationId)
                   .has_value());
  histogram_tester.ExpectUniqueTimeSample(
      kTimeUntilDefaultWithoutClickHistogram, base::Milliseconds(750), 1);
  histogram_tester.ExpectUniqueSample(
      kInteractionHistogram,
      DefaultBrowserInteractionType::kClosedOnDefaultRestored, 1);

  // Subsequent `IS_DEFAULT` updates should not record duplicate samples.
  task_environment().FastForwardBy(base::Seconds(1));
  TriggerStateChange(shell_integration::IS_DEFAULT);
  histogram_tester.ExpectTotalCount(kTimeUntilDefaultWithoutClickHistogram, 1);
  histogram_tester.ExpectTotalCount(kInteractionHistogram, 1);

  // A second loss-and-recovery cycle records a new sample cleanly.
  TriggerStateChange(shell_integration::NOT_DEFAULT);
  ASSERT_TRUE(display_service_tester()
                  .GetNotification(DefaultBrowserManager::kNotificationId)
                  .has_value());
  task_environment().FastForwardBy(base::Milliseconds(300));
  TriggerStateChange(shell_integration::IS_DEFAULT);
  EXPECT_FALSE(display_service_tester()
                   .GetNotification(DefaultBrowserManager::kNotificationId)
                   .has_value());
  histogram_tester.ExpectTotalCount(kTimeUntilDefaultWithoutClickHistogram, 2);
  histogram_tester.ExpectUniqueSample(
      kInteractionHistogram,
      DefaultBrowserInteractionType::kClosedOnDefaultRestored, 2);
}

TEST_F(DefaultBrowserNotificationObserverTest,
       DoesNotRecordMetricWhenNotificationDismissedViaButton) {
  base::HistogramTester histogram_tester;
  auto observer = CreateObserver(shell_integration::IS_DEFAULT);

  TriggerStateChange(shell_integration::NOT_DEFAULT);
  ASSERT_TRUE(display_service_tester()
                  .GetNotification(DefaultBrowserManager::kNotificationId)
                  .has_value());

  display_service_tester().SimulateClick(
      NotificationHandler::Type::DEFAULT_BROWSER_CHANGED,
      DefaultBrowserManager::kNotificationId, /*action_index=*/1,
      /*reply=*/std::nullopt);
  ASSERT_FALSE(display_service_tester()
                   .GetNotification(DefaultBrowserManager::kNotificationId)
                   .has_value());
  EXPECT_FALSE(DefaultBrowserChangedNotificationHandler::CloseNotification(
      &manager().GetProfile()));

  task_environment().FastForwardBy(base::Seconds(5));
  TriggerStateChange(shell_integration::IS_DEFAULT);
  histogram_tester.ExpectTotalCount(kTimeUntilDefaultWithoutClickHistogram, 0);
  histogram_tester.ExpectUniqueSample(
      kInteractionHistogram, DefaultBrowserInteractionType::kDismissed, 1);
}

TEST_F(DefaultBrowserNotificationObserverTest,
       DoesNotRecordMetricWhenNotificationClosedByUser) {
  base::HistogramTester histogram_tester;
  auto observer = CreateObserver(shell_integration::IS_DEFAULT);

  TriggerStateChange(shell_integration::NOT_DEFAULT);
  ASSERT_TRUE(display_service_tester()
                  .GetNotification(DefaultBrowserManager::kNotificationId)
                  .has_value());

  display_service_tester().RemoveNotification(
      NotificationHandler::Type::DEFAULT_BROWSER_CHANGED,
      DefaultBrowserManager::kNotificationId, /*by_user=*/true);
  ASSERT_FALSE(display_service_tester()
                   .GetNotification(DefaultBrowserManager::kNotificationId)
                   .has_value());
  EXPECT_FALSE(DefaultBrowserChangedNotificationHandler::CloseNotification(
      &manager().GetProfile()));

  task_environment().FastForwardBy(base::Seconds(5));
  TriggerStateChange(shell_integration::IS_DEFAULT);
  histogram_tester.ExpectTotalCount(kTimeUntilDefaultWithoutClickHistogram, 0);
  histogram_tester.ExpectUniqueSample(
      kInteractionHistogram, DefaultBrowserInteractionType::kDismissed, 1);
}

TEST_F(DefaultBrowserNotificationObserverTest,
       DoesNotRecordMetricWhenInitiallyNotDefault) {
  base::HistogramTester histogram_tester;
  auto observer = CreateObserver(shell_integration::NOT_DEFAULT);

  task_environment().FastForwardBy(base::Seconds(2));
  TriggerStateChange(shell_integration::IS_DEFAULT);

  histogram_tester.ExpectTotalCount(kTimeUntilDefaultWithoutClickHistogram, 0);
}

TEST_F(DefaultBrowserNotificationObserverTest,
       HandlesProfileDestructionBeforeBecomingDefault) {
  base::HistogramTester histogram_tester;
  auto observer = CreateObserver(shell_integration::IS_DEFAULT);

  TriggerStateChange(shell_integration::NOT_DEFAULT);
  ASSERT_TRUE(display_service_tester()
                  .GetNotification(DefaultBrowserManager::kNotificationId)
                  .has_value());

  DestroyProfile();

  task_environment().FastForwardBy(base::Seconds(1));
  TriggerStateChange(shell_integration::IS_DEFAULT);
  histogram_tester.ExpectTotalCount(kTimeUntilDefaultWithoutClickHistogram, 0);
}

TEST_F(DefaultBrowserNotificationObserverTest,
       HandlesDisplayServiceResetBeforeBecomingDefault) {
  base::HistogramTester histogram_tester;
  auto observer = CreateObserver(shell_integration::IS_DEFAULT);

  TriggerStateChange(shell_integration::NOT_DEFAULT);
  ASSERT_TRUE(display_service_tester()
                  .GetNotification(DefaultBrowserManager::kNotificationId)
                  .has_value());

  // Destroying `NotificationDisplayServiceTester` while `profile_` is still
  // alive installs a null `TestingFactory` on the live profile (mirroring
  // `InProcessBrowserTest` teardown order).
  ResetDisplayServiceTester();

  task_environment().FastForwardBy(base::Seconds(1));
  TriggerStateChange(shell_integration::IS_DEFAULT);
  EXPECT_FALSE(DefaultBrowserChangedNotificationHandler::CloseNotification(
      &manager().GetProfile()));
  histogram_tester.ExpectTotalCount(kTimeUntilDefaultWithoutClickHistogram, 0);
}

}  // namespace default_browser
