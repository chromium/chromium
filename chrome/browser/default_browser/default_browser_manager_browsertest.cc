// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/default_browser_notification_observer.h"
#include "chrome/browser/default_browser/test_support/fake_shell_delegate.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#endif  // BUILDFLAG(IS_WIN)

namespace default_browser {

namespace {

#if BUILDFLAG(IS_WIN)
constexpr wchar_t kProgIdValue[] = L"ProgId";

constexpr wchar_t kRegistryPath[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\h"
    L"ttp\\UserChoice";

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

#if BUILDFLAG(IS_WIN)
class DefaultBrowserManagerWinBrowserTest : public InProcessBrowserTest {
 protected:
  DefaultBrowserManagerWinBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enable_features*/ {kDefaultBrowserFramework,
                             kDefaultBrowserChangedOsNotification},
        /*disabled_features=*/{});
  }
  ~DefaultBrowserManagerWinBrowserTest() override = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(HKEY_CURRENT_USER, kRegistryPath, KEY_WRITE));
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(
            browser()->profile());
  }

  void TearDownOnMainThread() override {
    display_service_tester_.reset();
    fake_shell_delegate_ptr_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    global_feature_override_ =
        GlobalFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindLambdaForTesting([&](BrowserProcess& browser_process) {
              auto fake_shell_delegate = std::make_unique<FakeShellDelegate>();
              fake_shell_delegate_ptr_ = fake_shell_delegate.get();
              return std::make_unique<DefaultBrowserManager>(
                  &browser_process, std::move(fake_shell_delegate),
                  base::BindLambdaForTesting(
                      [&]() { return browser()->profile(); }));
            }));
  }

  // Helper to simulate the environment change that triggers the notification.
  void TriggerNotification() {
    // 1. Ensure Chrome believes it is NOT default.
    fake_shell_delegate_ptr_->set_default_state(shell_integration::IS_DEFAULT);

    // 2. Set up the registry to look like Chrome was default initially (so the
    //    change to something else is significant).
    CreateDefaultBrowserKey(L"ChromeHTML");
    content::RunAllTasksUntilIdle();

    // 3. Prepare to listen for the async monitor event.
    base::test::TestFuture<DefaultBrowserState> monitor_future;
    auto* manager = DefaultBrowserManager::From(g_browser_process);
    base::CallbackListSubscription sub = manager->RegisterDefaultBrowserChanged(
        monitor_future.GetRepeatingCallback());

    // 4. Simulate external registry change to a different browser.
    ChangeDefaultBrowserProgId(L"VanadiumHTML");

    // 5. Wait for the manager to detect the change and update state.
    fake_shell_delegate_ptr_->set_default_state(shell_integration::NOT_DEFAULT);
    EXPECT_EQ(monitor_future.Take(), shell_integration::NOT_DEFAULT);

    // 6. Allow the notification tasks to post to the UI thread.
    content::RunAllTasksUntilIdle();
  }

  void CreateDefaultBrowserKey(const std::wstring& prog_id) {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(HKEY_CURRENT_USER, kRegistryPath, KEY_WRITE));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kProgIdValue, prog_id.c_str()));
  }

  void ChangeDefaultBrowserProgId(const std::wstring& prog_id) {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Open(HKEY_CURRENT_USER, kRegistryPath, KEY_WRITE));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kProgIdValue, prog_id.c_str()));
  }

  raw_ptr<FakeShellDelegate> fake_shell_delegate_ptr_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::UserDataFactory::ScopedOverride global_feature_override_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       ChangeIsDetectedAndObserverIsNotified) {
  CreateDefaultBrowserKey(L"ChromeHTML");
  DefaultBrowserManager* manager =
      DefaultBrowserManager::From(g_browser_process);
  ASSERT_TRUE(manager);

  base::test::TestFuture<DefaultBrowserState> future;
  base::CallbackListSubscription subscription =
      manager->RegisterDefaultBrowserChanged(future.GetRepeatingCallback());
  ChangeDefaultBrowserProgId(L"VanadiumHTML");
  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       SubscriptionDestroyedPreventsCallback) {
  CreateDefaultBrowserKey(L"ChromeHTML");
  DefaultBrowserManager* manager =
      DefaultBrowserManager::From(g_browser_process);
  ASSERT_TRUE(manager);

  base::test::TestFuture<DefaultBrowserState> future;
  {
    base::CallbackListSubscription subscription =
        manager->RegisterDefaultBrowserChanged(future.GetRepeatingCallback());
  }

  ChangeDefaultBrowserProgId(L"VanadiumHTML");
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(future.IsReady());
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       AllRegisteredObserversAreNotified) {
  CreateDefaultBrowserKey(L"ChromeHTML");
  DefaultBrowserManager* manager =
      DefaultBrowserManager::From(g_browser_process);
  ASSERT_TRUE(manager);

  base::test::TestFuture<DefaultBrowserState> future1;
  base::test::TestFuture<DefaultBrowserState> future2;
  base::CallbackListSubscription subscription1 =
      manager->RegisterDefaultBrowserChanged(future1.GetRepeatingCallback());
  base::CallbackListSubscription subscription2 =
      manager->RegisterDefaultBrowserChanged(future2.GetRepeatingCallback());

  ChangeDefaultBrowserProgId(L"VanadiumHTML");
  EXPECT_TRUE(future1.Wait());
  EXPECT_TRUE(future2.Wait());
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       SubsequentChangesAreAlsoDetected) {
  CreateDefaultBrowserKey(L"ChromeHTML");
  DefaultBrowserManager* manager =
      DefaultBrowserManager::From(g_browser_process);
  ASSERT_TRUE(manager);

  {
    base::test::TestFuture<DefaultBrowserState> future_change1;
    base::CallbackListSubscription subscription =
        manager->RegisterDefaultBrowserChanged(
            future_change1.GetRepeatingCallback());
    ChangeDefaultBrowserProgId(L"VanadiumHTML");
    ASSERT_TRUE(future_change1.Wait()) << "First change was not detected.";
  }

  {
    base::test::TestFuture<DefaultBrowserState> future_change2;
    base::CallbackListSubscription subscription =
        manager->RegisterDefaultBrowserChanged(
            future_change2.GetRepeatingCallback());
    ChangeDefaultBrowserProgId(L"ManganeseHTML");
    EXPECT_TRUE(future_change2.Wait()) << "Second change was not detected.";
  }
}

IN_PROC_BROWSER_TEST_F(
    DefaultBrowserManagerWinBrowserTest,
    SubsequentChangesAreAlsoDetectedWithTheSameSubscription) {
  CreateDefaultBrowserKey(L"ChromeHTML");
  DefaultBrowserManager* manager =
      DefaultBrowserManager::From(g_browser_process);
  ASSERT_TRUE(manager);

  int call_count = 0;
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;

  // This callback will be invoked for each default browser change.
  // On the first invocation, it will quit the first `run_loop`.
  // On the second, it will quit the second `run_loop`.
  auto subscription_callback = base::BindRepeating(
      [](int* count, base::RunLoop* loop1, base::RunLoop* loop2,
         DefaultBrowserState state) {
        (*count)++;
        if (*count == 1) {
          loop1->Quit();
        } else if (*count == 2) {
          loop2->Quit();
        }
      },
      &call_count, &run_loop1, &run_loop2);

  // Register the single subscription that will be used for the entire test.
  base::CallbackListSubscription subscription =
      manager->RegisterDefaultBrowserChanged(subscription_callback);

  // Trigger the first change.
  ChangeDefaultBrowserProgId(L"VanadiumHTML");
  run_loop1.Run();
  EXPECT_EQ(call_count, 1) << "First change was not detected.";

  // Trigger the second change. The same subscription should be notified again.
  ChangeDefaultBrowserProgId(L"ManganeseHTML");
  run_loop2.Run();
  EXPECT_EQ(call_count, 2) << "Second change was not detected.";
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       RegistryChangeTriggersSystemNotification) {
  TriggerNotification();

  auto notification = display_service_tester_->GetNotification(
      DefaultBrowserManager::kNotificationId);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(),
            l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_TITLE));
  EXPECT_EQ(notification->message(),
            l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_MESSAGE));

  histogram_tester_.ExpectUniqueSample(
      "DefaultBrowser.ChangeDetectedNotification.ShellIntegration.Shown", 1, 1);
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       NoNotificationWhenChromeRemainsDefault) {
  fake_shell_delegate_ptr_->set_default_state(shell_integration::IS_DEFAULT);
  CreateDefaultBrowserKey(L"ChromeHTML");

  base::test::TestFuture<DefaultBrowserState> monitor_future;
  auto* manager = DefaultBrowserManager::From(g_browser_process);
  base::CallbackListSubscription sub = manager->RegisterDefaultBrowserChanged(
      monitor_future.GetRepeatingCallback());

  ChangeDefaultBrowserProgId(L"ChromeHTML");
  EXPECT_EQ(monitor_future.Take(), shell_integration::IS_DEFAULT);

  content::RunAllTasksUntilIdle();

  auto notification = display_service_tester_->GetNotification(
      DefaultBrowserManager::kNotificationId);
  EXPECT_FALSE(notification.has_value())
      << "Notification shown even though Chrome is default.";
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       ClickAcceptTriggersSetterAndMetric) {
  TriggerNotification();

  ASSERT_TRUE(display_service_tester_->GetNotification(
      DefaultBrowserManager::kNotificationId));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::DEFAULT_BROWSER_CHANGED,
      DefaultBrowserManager::kNotificationId,
      /*action_index=*/0,
      /*reply=*/std::nullopt);

  histogram_tester_.ExpectUniqueSample(
      "DefaultBrowser.ChangeDetectedNotification.ShellIntegration.Interaction",
      DefaultBrowserInteractionType::kAccepted, 1);

  histogram_tester_.ExpectBucketCount(
      "DefaultBrowser.ChangeDetectedNotification.ShellIntegration.Interaction",
      DefaultBrowserInteractionType::kDismissed, 0);

  EXPECT_FALSE(display_service_tester_->GetNotification(
      DefaultBrowserManager::kNotificationId));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       ClickNoThanksDismissesAndRecordsMetric) {
  TriggerNotification();

  ASSERT_TRUE(display_service_tester_->GetNotification(
      DefaultBrowserManager::kNotificationId));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::DEFAULT_BROWSER_CHANGED,
      DefaultBrowserManager::kNotificationId,
      /*action_index=*/1,
      /*reply=*/std::nullopt);

  histogram_tester_.ExpectUniqueSample(
      "DefaultBrowser.ChangeDetectedNotification.ShellIntegration.Interaction",
      DefaultBrowserInteractionType::kDismissed, 1);

  histogram_tester_.ExpectBucketCount(
      "DefaultBrowser.ChangeDetectedNotification.ShellIntegration.Interaction",
      DefaultBrowserInteractionType::kAccepted, 0);

  EXPECT_FALSE(display_service_tester_->GetNotification(
      DefaultBrowserManager::kNotificationId));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       UserCloseDismissesAndRecordsMetric) {
  TriggerNotification();

  ASSERT_TRUE(display_service_tester_->GetNotification(
      DefaultBrowserManager::kNotificationId));

  display_service_tester_->RemoveNotification(
      NotificationHandler::Type::DEFAULT_BROWSER_CHANGED,
      DefaultBrowserManager::kNotificationId,
      /*by_user=*/true);

  histogram_tester_.ExpectUniqueSample(
      "DefaultBrowser.ChangeDetectedNotification.ShellIntegration.Interaction",
      DefaultBrowserInteractionType::kDismissed, 1);

  histogram_tester_.ExpectBucketCount(
      "DefaultBrowser.ChangeDetectedNotification.ShellIntegration.Interaction",
      DefaultBrowserInteractionType::kAccepted, 0);
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       NoNotificationWhenRemainingNotDefault) {
  fake_shell_delegate_ptr_->set_default_state(shell_integration::NOT_DEFAULT);
  CreateDefaultBrowserKey(L"EdgeHTML");

  content::RunAllTasksUntilIdle();

  base::test::TestFuture<DefaultBrowserState> monitor_future;
  auto* manager = DefaultBrowserManager::From(g_browser_process);
  base::CallbackListSubscription sub = manager->RegisterDefaultBrowserChanged(
      monitor_future.GetRepeatingCallback());

  ChangeDefaultBrowserProgId(L"VanadiumHTML");

  EXPECT_EQ(monitor_future.Take(), shell_integration::NOT_DEFAULT);
  content::RunAllTasksUntilIdle();

  auto notification = display_service_tester_->GetNotification(
      DefaultBrowserManager::kNotificationId);
  EXPECT_FALSE(notification.has_value());
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       NoNotificationWhenBecomingDefault) {
  fake_shell_delegate_ptr_->set_default_state(shell_integration::NOT_DEFAULT);
  CreateDefaultBrowserKey(L"EdgeHTML");
  content::RunAllTasksUntilIdle();

  base::test::TestFuture<DefaultBrowserState> monitor_future;
  auto* manager = DefaultBrowserManager::From(g_browser_process);
  base::CallbackListSubscription sub = manager->RegisterDefaultBrowserChanged(
      monitor_future.GetRepeatingCallback());

  fake_shell_delegate_ptr_->set_default_state(shell_integration::IS_DEFAULT);
  ChangeDefaultBrowserProgId(L"ChromeHTML");

  EXPECT_EQ(monitor_future.Take(), shell_integration::IS_DEFAULT);
  content::RunAllTasksUntilIdle();

  auto notification = display_service_tester_->GetNotification(
      DefaultBrowserManager::kNotificationId);
  EXPECT_FALSE(notification.has_value());
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerWinBrowserTest,
                       NotificationShownOnTransitionFromDefault) {
  fake_shell_delegate_ptr_->set_default_state(shell_integration::IS_DEFAULT);
  CreateDefaultBrowserKey(L"ChromeHTML");
  content::RunAllTasksUntilIdle();

  base::test::TestFuture<DefaultBrowserState> monitor_future;
  auto* manager = DefaultBrowserManager::From(g_browser_process);
  base::CallbackListSubscription sub = manager->RegisterDefaultBrowserChanged(
      monitor_future.GetRepeatingCallback());

  fake_shell_delegate_ptr_->set_default_state(shell_integration::NOT_DEFAULT);
  ChangeDefaultBrowserProgId(L"VanadiumHTML");

  EXPECT_EQ(monitor_future.Take(), shell_integration::NOT_DEFAULT);
  content::RunAllTasksUntilIdle();

  auto notification = display_service_tester_->GetNotification(
      DefaultBrowserManager::kNotificationId);
  EXPECT_TRUE(notification.has_value());
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace default_browser
