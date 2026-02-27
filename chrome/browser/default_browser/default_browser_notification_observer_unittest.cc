// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_notification_observer.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/default_browser/test_support/fake_shell_delegate.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

namespace default_browser {

class DefaultBrowserNotificationObserverTest : public testing::Test {
 protected:
  DefaultBrowserNotificationObserverTest() {
    feature_list_.InitWithFeatures(
        {kDefaultBrowserChangedOsNotification, kDefaultBrowserFramework}, {});
  }

  void SetUp() override {
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(&profile_);

    global_feature_override_ =
        GlobalFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindLambdaForTesting([&](BrowserProcess& browser_process) {
              auto fake_delegate = std::make_unique<FakeShellDelegate>();
              fake_delegate->set_default_state(shell_integration::IS_DEFAULT);
              fake_shell_delegate_ptr_ = fake_delegate.get();
              return std::make_unique<DefaultBrowserManager>(
                  TestingBrowserProcess::GetGlobal(), std::move(fake_delegate),
                  base::BindLambdaForTesting(
                      [&]() -> Profile* { return &profile_; }));
            }));

    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/false);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  DefaultBrowserManager& manager() {
    return *DefaultBrowserManager::From(TestingBrowserProcess::GetGlobal());
  }

  NotificationDisplayServiceTester& display_service_tester() {
    return *display_service_tester_;
  }

  // Helper method to create the observer and capture the callback it uses
  // to register for state changes. This allows us to trigger changes directly.
  std::unique_ptr<DefaultBrowserNotificationObserver> CreateObserver(
      DefaultBrowserState initial_state = shell_integration::IS_DEFAULT) {
    return std::make_unique<DefaultBrowserNotificationObserver>(
        /*register_callback=*/base::BindLambdaForTesting(
            [&](base::RepeatingCallback<void(DefaultBrowserState)> cb) {
              // Capture the observer's callback so we can simulate broadcasts.
              state_change_callback_ = std::move(cb);
              return base::CallbackListSubscription();
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
    ASSERT_TRUE(state_change_callback_) << "Observer not initialized yet.";
    state_change_callback_.Run(new_state);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;

  ui::UserDataFactory::ScopedOverride global_feature_override_;
  raw_ptr<FakeShellDelegate> fake_shell_delegate_ptr_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

  // Stores the callback bound to the observer's OnDefaultBrowserStateChanged.
  base::RepeatingCallback<void(DefaultBrowserState)> state_change_callback_;
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

}  // namespace default_browser
