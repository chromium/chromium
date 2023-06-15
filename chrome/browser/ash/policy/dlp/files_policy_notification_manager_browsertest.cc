// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/enterprise/data_controls/component.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy {

namespace {

constexpr char kExampleUrl[] = "https://example1.com";
constexpr char kNotificationId[] = "dlp_files_0";

}  // namespace

class FilesPolicyNotificationManagerBrowserTest : public InProcessBrowserTest {
 public:
  FilesPolicyNotificationManagerBrowserTest() = default;
  FilesPolicyNotificationManagerBrowserTest(
      const FilesPolicyNotificationManagerBrowserTest&) = delete;
  FilesPolicyNotificationManagerBrowserTest& operator=(
      const FilesPolicyNotificationManagerBrowserTest&) = delete;
  ~FilesPolicyNotificationManagerBrowserTest() override = default;
};

// (b/273269211): This is a test for the crash that happens upon showing a
// warning dialog when a file is moved to Google Drive.
IN_PROC_BROWSER_TEST_F(FilesPolicyNotificationManagerBrowserTest,
                       WarningDialog_ComponentDestination) {
  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);
  std::vector<base::FilePath> warning_files;
  warning_files.emplace_back(base::FilePath("file1.txt"));
  fpnm->ShowDlpWarning(base::DoNothing(), absl::nullopt,
                       std::move(warning_files),
                       DlpFileDestination(data_controls::Component::kDrive),
                       dlp::FileAction::kMove);
}

// (b/277594200): This is a test for the crash that happens upon showing a
// warning dialog when a file is dragged to a webpage.
IN_PROC_BROWSER_TEST_F(FilesPolicyNotificationManagerBrowserTest,
                       WarningDialog_UrlDestination) {
  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);
  std::vector<base::FilePath> warning_files;
  warning_files.emplace_back(base::FilePath("file1.txt"));
  fpnm->ShowDlpWarning(base::DoNothing(), absl::nullopt,
                       std::move(warning_files),
                       DlpFileDestination(kExampleUrl), dlp::FileAction::kMove);
}

// (b/281495499): This is a test for the crash that happens upon showing a
// warning dialog for downloads.
IN_PROC_BROWSER_TEST_F(FilesPolicyNotificationManagerBrowserTest,
                       WarningDialog_Download) {
  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);
  std::vector<base::FilePath> warning_files;
  warning_files.emplace_back(base::FilePath("file1.txt"));
  fpnm->ShowDlpWarning(base::DoNothing(), absl::nullopt,
                       std::move(warning_files),
                       DlpFileDestination(data_controls::Component::kDrive),
                       dlp::FileAction::kDownload);
}

using BlockedFilesMap = std::map<DlpConfidentialFile, Policy>;

class MockFilesPolicyDialogFactory : public FilesPolicyDialogFactory {
 public:
  MOCK_METHOD(views::Widget*,
              CreateWarnDialog,
              (OnDlpRestrictionCheckedCallback callback,
               const std::vector<DlpConfidentialFile>&,
               DlpFileDestination,
               dlp::FileAction,
               gfx::NativeWindow),
              (override));

  MOCK_METHOD(views::Widget*,
              CreateErrorDialog,
              (const BlockedFilesMap&,
               DlpFileDestination,
               dlp::FileAction,
               gfx::NativeWindow),
              (override));
};

// NotificationPlatformBridgeDelegator test implementation. Keeps track of
// displayed notifications and allows clicking on a displayed notification.
class TestNotificationPlatformBridgeDelegator
    : public NotificationPlatformBridgeDelegator {
 public:
  explicit TestNotificationPlatformBridgeDelegator(Profile* profile)
      : NotificationPlatformBridgeDelegator(profile, base::DoNothing()) {}
  ~TestNotificationPlatformBridgeDelegator() override = default;

  // NotificationPlatformBridgeDelegator:
  void Display(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) override {
    notifications_.emplace(notification.id(), notification);
    ids_.insert(notification.id());
  }

  void Close(NotificationHandler::Type notification_type,
             const std::string& notification_id) override {
    notifications_.erase(notification_id);
    ids_.erase(notification_id);
  }

  void GetDisplayed(GetDisplayedNotificationsCallback callback) const override {
    std::move(callback).Run(ids_, /*supports_sync=*/true);
  }

  absl::optional<message_center::Notification> GetDisplayedNotification(
      const std::string& notification_id) {
    auto it = notifications_.find(notification_id);
    if (it != notifications_.end()) {
      return it->second;
    }
    return absl::nullopt;
  }

  // If a notification with `notification_id` is displayed, simulates clicking
  // on that notification with `button_index` button.
  void Click(const std::string& notification_id,
             absl::optional<int> button_index) {
    auto it = notifications_.find(notification_id);
    if (it == notifications_.end()) {
      return;
    }
    it->second.delegate()->Click(button_index, absl::nullopt);
  }

 private:
  std::map<std::string, message_center::Notification> notifications_;
  std::set<std::string> ids_;
};

class OnNotificationClickedTest
    : public FilesPolicyNotificationManagerBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<dlp::FileAction, DlpFileDestination>> {
 public:
  void SetUpOnMainThread() override {
    FilesPolicyNotificationManagerBrowserTest::SetUpOnMainThread();

    // Needed to check that Files app was/wasn't opened.
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(
        browser()->profile());

    display_service_ = static_cast<NotificationDisplayServiceImpl*>(
        NotificationDisplayServiceFactory::GetForProfile(browser()->profile()));
    auto bridge = std::make_unique<TestNotificationPlatformBridgeDelegator>(
        browser()->profile());
    bridge_ = bridge.get();
    display_service_->SetNotificationPlatformBridgeDelegatorForTesting(
        std::move(bridge));

    factory_ = std::make_unique<MockFilesPolicyDialogFactory>();
    FilesPolicyDialog::SetFactory(factory_.get());

    DlpFilesController::SetNewFilesPolicyUXEnabledForTesting(
        /*is_enabled=*/true);
  }

 protected:
  FilesPolicyDialogFactory* factory() { return factory_.get(); }

  // Returns the last active Files app window, or nullptr when none are found.
  Browser* FindFilesApp() {
    return FindSystemWebAppBrowser(browser()->profile(),
                                   ash::SystemWebAppType::FILE_MANAGER);
  }

  NotificationDisplayServiceImpl* display_service_;
  TestNotificationPlatformBridgeDelegator* bridge_;
  std::unique_ptr<MockFilesPolicyDialogFactory> factory_;
};

class OnWarningNotificationClickedTest : public OnNotificationClickedTest {};

// Tests that clicking the OK button on a warning notification for a single file
// continues the action without showing the dialog.
IN_PROC_BROWSER_TEST_P(OnWarningNotificationClickedTest,
                       SingleFileOKContinues) {
  auto [action, destination] = GetParam();
  EXPECT_CALL(*factory_, CreateWarnDialog).Times(0);
  // No Files app opened.
  ASSERT_FALSE(FindSystemWebAppBrowser(browser()->profile(),
                                       ash::SystemWebAppType::FILE_MANAGER));

  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  // The callback is invoked directly from the notification.
  base::MockCallback<OnDlpRestrictionCheckedCallback> cb;
  EXPECT_CALL(cb, Run(/*should_proceed=*/true)).Times(1);

  fpnm->ShowDlpWarning(cb.Get(), /*task_id=*/absl::nullopt,
                       {base::FilePath("file1.txt")}, destination, action);

  ASSERT_TRUE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
  bridge_->Click(kNotificationId, NotificationButton::OK);

  // No Files app opened.
  ASSERT_FALSE(FindSystemWebAppBrowser(browser()->profile(),
                                       ash::SystemWebAppType::FILE_MANAGER));

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
}

// Tests that clicking the OK button on a warning notification for multiple
// files shows a dialog instead of continuing the action.
IN_PROC_BROWSER_TEST_P(OnWarningNotificationClickedTest,
                       MultiFileOKShowsDialog) {
  auto [action, destination] = GetParam();
  std::vector<base::FilePath> warning_files;
  warning_files.emplace_back("file1.txt");
  warning_files.emplace_back("file2.txt");
  EXPECT_CALL(*factory_, CreateWarnDialog(
                             base::test::IsNotNullCallback(),
                             std::vector<DlpConfidentialFile>(
                                 {warning_files.begin(), warning_files.end()}),
                             destination, action, testing::NotNull()))
      .Times(2);

  // No Files app opened.
  ASSERT_FALSE(FindFilesApp());

  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  // The callback shouldn't be invoked.
  base::MockCallback<OnDlpRestrictionCheckedCallback> cb;
  EXPECT_CALL(cb, Run).Times(0);

  fpnm->ShowDlpWarning(cb.Get(), /*task_id=*/absl::nullopt, warning_files,
                       destination, action);

  ASSERT_TRUE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
  bridge_->Click(kNotificationId, NotificationButton::OK);

  // Check that a new Files app is opened.
  Browser* first_app = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_EQ(first_app, FindFilesApp());

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId).has_value());

  // Show another notification and dialog. Another app should be opened.
  fpnm->ShowDlpWarning(cb.Get(), /*task_id=*/absl::nullopt, warning_files,
                       destination, action);

  const std::string second_notification = "dlp_files_1";
  ASSERT_TRUE(
      bridge_->GetDisplayedNotification(second_notification).has_value());
  bridge_->Click(second_notification, NotificationButton::OK);

  // Check that a new Files app is opened.
  Browser* second_app = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_EQ(second_app, FindFilesApp());
  EXPECT_NE(first_app, second_app);
}

// Tests that clicking the OK button on a warning notification for multiple
// files shows a system modal dialog when Files app doesn't launch before
// timeout.
IN_PROC_BROWSER_TEST_P(OnWarningNotificationClickedTest,
                       MultiFileOKShowsDialog_Timeout) {
  auto [action, destination] = GetParam();
  std::vector<base::FilePath> warning_files;
  warning_files.emplace_back("file1.txt");
  warning_files.emplace_back("file2.txt");
  // Null modal parent means the dialog is a system modal.
  EXPECT_CALL(*factory_, CreateWarnDialog(
                             base::test::IsNotNullCallback(),
                             std::vector<DlpConfidentialFile>(
                                 {warning_files.begin(), warning_files.end()}),
                             destination, action, testing::IsNull()))
      .Times(1);

  // No Files app opened.
  ASSERT_FALSE(FindFilesApp());

  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  fpnm->SetTaskRunnerForTesting(task_runner);

  // The callback shouldn't be invoked.
  base::MockCallback<OnDlpRestrictionCheckedCallback> cb;
  EXPECT_CALL(cb, Run).Times(0);

  fpnm->ShowDlpWarning(cb.Get(), /*task_id=*/absl::nullopt, warning_files,
                       destination, action);

  ASSERT_TRUE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
  bridge_->Click(kNotificationId, NotificationButton::OK);

  // Skip the timeout.
  task_runner->FastForwardBy(base::TimeDelta(base::Milliseconds(3000)));

  // Check that a new Files app is still opened.
  ASSERT_EQ(ui_test_utils::WaitForBrowserToOpen(), FindFilesApp());

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
}

// Tests that clicking the Cancel button on a warning notification cancels the
// action without showing the dialog.
IN_PROC_BROWSER_TEST_P(OnWarningNotificationClickedTest, CancelShowsNoDialog) {
  auto [action, destination] = GetParam();
  EXPECT_CALL(*factory_, CreateWarnDialog).Times(0);

  // No Files app opened.
  ASSERT_FALSE(FindFilesApp());

  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  // The callback is invoked directly from the notification.
  base::MockCallback<OnDlpRestrictionCheckedCallback> cb;
  EXPECT_CALL(cb, Run(/*should_proceed=*/false)).Times(1);

  fpnm->ShowDlpWarning(
      cb.Get(), /*task_id=*/absl::nullopt,
      {base::FilePath("file1.txt"), base::FilePath("file2.txt")}, destination,
      action);

  ASSERT_TRUE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
  bridge_->Click(kNotificationId, NotificationButton::CANCEL);

  // No Files app opened.
  ASSERT_FALSE(FindFilesApp());

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
}

INSTANTIATE_TEST_SUITE_P(
    FPNM,
    OnWarningNotificationClickedTest,
    ::testing::Values(
        std::make_tuple(dlp::FileAction::kUpload,
                        DlpFileDestination(kExampleUrl)),
        std::make_tuple(dlp::FileAction::kMove,
                        DlpFileDestination(data_controls::Component::kDrive))));

class OnErrorNotificationClickedTest : public OnNotificationClickedTest {};

// Tests that clicking the OK button on an error notification for multiple-
// files shows a dialog.
IN_PROC_BROWSER_TEST_P(OnErrorNotificationClickedTest, MultiFileOKShowsDialog) {
  auto [action, destination] = GetParam();
  BlockedFilesMap blocked_map;
  blocked_map.emplace(base::FilePath("file1.txt"), Policy::kDlp);
  blocked_map.emplace(base::FilePath("file2.txt"), Policy::kDlp);
  EXPECT_CALL(*factory_, CreateErrorDialog(blocked_map, testing::_, action,
                                           testing::NotNull()))
      .Times(1);

  // No Files app opened.
  ASSERT_FALSE(FindFilesApp());

  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  std::vector<base::FilePath> blocked_files;
  blocked_files.emplace_back("file1.txt");
  blocked_files.emplace_back("file2.txt");
  fpnm->ShowDlpBlockedFiles(absl::nullopt, std::move(blocked_files), action);

  ASSERT_TRUE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
  bridge_->Click(kNotificationId, NotificationButton::OK);

  // Check that a new Files app is opened.
  ASSERT_EQ(ui_test_utils::WaitForBrowserToOpen(), FindFilesApp());

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
}

// Tests that clicking the OK button on an error notification for multiple
// files shows a system modal dialog when Files app doesn't launch before
// timeout.
IN_PROC_BROWSER_TEST_P(OnErrorNotificationClickedTest,
                       MultiFileOKShowsDialog_Timeout) {
  auto [action, destination] = GetParam();
  BlockedFilesMap blocked_map;
  blocked_map.emplace(base::FilePath("file1.txt"), Policy::kDlp);
  blocked_map.emplace(base::FilePath("file2.txt"), Policy::kDlp);
  // Null modal parent means the dialog is a system modal.
  EXPECT_CALL(*factory_, CreateErrorDialog(blocked_map, testing::_, action,
                                           testing::IsNull()))
      .Times(1);

  // No Files app opened.
  ASSERT_FALSE(FindFilesApp());

  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  fpnm->SetTaskRunnerForTesting(task_runner);

  std::vector<base::FilePath> blocked_files;
  blocked_files.emplace_back("file1.txt");
  blocked_files.emplace_back("file2.txt");
  fpnm->ShowDlpBlockedFiles(absl::nullopt, std::move(blocked_files), action);

  ASSERT_TRUE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
  bridge_->Click(kNotificationId, NotificationButton::OK);
  // Skip the timeout.
  task_runner->FastForwardBy(base::TimeDelta(base::Milliseconds(3000)));

  // Check that a new Files app is still opened.
  ASSERT_EQ(ui_test_utils::WaitForBrowserToOpen(), FindFilesApp());

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
}

// Tests that clicking the Cancel button on an error notification dismisses the
// notification without showing the dialog.
IN_PROC_BROWSER_TEST_P(OnErrorNotificationClickedTest, CancelDismisses) {
  auto [action, destination] = GetParam();
  EXPECT_CALL(*factory_, CreateErrorDialog).Times(0);

  // No Files app opened.
  ASSERT_FALSE(FindFilesApp());

  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  std::vector<base::FilePath> blocked_files;
  blocked_files.emplace_back("file1.txt");
  blocked_files.emplace_back("file2.txt");
  fpnm->ShowDlpBlockedFiles(absl::nullopt, std::move(blocked_files), action);

  ASSERT_TRUE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
  bridge_->Click(kNotificationId, NotificationButton::CANCEL);

  // No Files app opened.
  ASSERT_FALSE(FindFilesApp());

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
}

INSTANTIATE_TEST_SUITE_P(
    FPNM,
    OnErrorNotificationClickedTest,
    ::testing::Values(
        std::make_tuple(dlp::FileAction::kOpen,
                        DlpFileDestination(kExampleUrl)),
        std::make_tuple(dlp::FileAction::kDownload,
                        DlpFileDestination(data_controls::Component::kUsb))));

}  // namespace policy
