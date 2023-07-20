// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_test_utils.h"
#include "chrome/browser/ash/policy/dlp/mock_dlp_files_controller_ash.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/data_controls/component.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy {

namespace {

using testing::Field;

using policy::AddCopyOrMoveIOTask;
using policy::kNotificationId;

constexpr char kExampleUrl[] = "https://example1.com";
const file_manager::io_task::IOTaskId kTaskId1 = 1u;
const file_manager::io_task::IOTaskId kTaskId2 = 2u;
constexpr char kNotificationId1[] = "swa-file-operation-1";
constexpr char kNotificationId2[] = "swa-file-operation-2";

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

using BlockedFilesMap = std::map<DlpConfidentialFile, Policy>;

class MockFilesPolicyDialogFactory : public FilesPolicyDialogFactory {
 public:
  MOCK_METHOD(views::Widget*,
              CreateWarnDialog,
              (OnDlpRestrictionCheckedCallback callback,
               const std::vector<DlpConfidentialFile>&,
               dlp::FileAction,
               gfx::NativeWindow,
               absl::optional<DlpFileDestination>),
              (override));

  MOCK_METHOD(views::Widget*,
              CreateErrorDialog,
              (const BlockedFilesMap&, dlp::FileAction, gfx::NativeWindow),
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
    : public FilesPolicyNotificationManagerBrowserTest {
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

  raw_ptr<NotificationDisplayServiceImpl, ExperimentalAsh> display_service_;
  raw_ptr<TestNotificationPlatformBridgeDelegator, ExperimentalAsh> bridge_;
  std::unique_ptr<MockFilesPolicyDialogFactory> factory_;
};

class OnDlpWarningNotificationClickedTest
    : public OnNotificationClickedTest,
      public ::testing::WithParamInterface<
          std::tuple<dlp::FileAction, DlpFileDestination>> {};

// Tests that clicking on the warning notification, but no button is ignored.
IN_PROC_BROWSER_TEST_P(OnDlpWarningNotificationClickedTest,
                       SingleFileNoButtonIgnored) {
  auto [action, destination] = GetParam();
  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  // The callback is not invoked.
  base::MockCallback<OnDlpRestrictionCheckedCallback> cb;
  EXPECT_CALL(cb, Run).Times(0);

  fpnm->ShowDlpWarning(cb.Get(), /*task_id=*/absl::nullopt,
                       {base::FilePath("file1.txt")}, destination, action);

  ASSERT_TRUE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
  bridge_->Click(kNotificationId, /*button_index=*/absl::nullopt);
  // The notification shouldn't be closed.
  EXPECT_TRUE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
}

// Tests that closing the warning notification (e.g. by X or Dismiss all)
// invokes the Cancel callback.
IN_PROC_BROWSER_TEST_P(OnDlpWarningNotificationClickedTest,
                       SingleFileCloseCancels) {
  auto [action, destination] = GetParam();
  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  // The task is cancelled.
  base::MockCallback<OnDlpRestrictionCheckedCallback> cb;
  EXPECT_CALL(cb, Run(/*should_proceed=*/false)).Times(1);

  fpnm->ShowDlpWarning(cb.Get(), /*task_id=*/absl::nullopt,
                       {base::FilePath("file1.txt")}, destination, action);

  auto notification = bridge_->GetDisplayedNotification(kNotificationId);
  ASSERT_TRUE(notification.has_value());
  notification->delegate()->Close(
      /*by_user=*/true);  // parameter doesn't matter
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
}

// Tests that clicking the OK button on a warning notification for a single
// file continues the action without showing the dialog.
IN_PROC_BROWSER_TEST_P(OnDlpWarningNotificationClickedTest,
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
IN_PROC_BROWSER_TEST_P(OnDlpWarningNotificationClickedTest,
                       MultiFileOKShowsDialog) {
  auto [action, destination] = GetParam();
  std::vector<base::FilePath> warning_files;
  warning_files.emplace_back("file1.txt");
  warning_files.emplace_back("file2.txt");
  EXPECT_CALL(
      *factory_,
      CreateWarnDialog(base::test::IsNotNullCallback(),
                       std::vector<DlpConfidentialFile>(
                           {warning_files.begin(), warning_files.end()}),
                       action, testing::NotNull(), testing::Eq(absl::nullopt)))
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
IN_PROC_BROWSER_TEST_P(OnDlpWarningNotificationClickedTest,
                       MultiFileOKShowsDialog_Timeout) {
  auto [action, destination] = GetParam();
  std::vector<base::FilePath> warning_files;
  warning_files.emplace_back("file1.txt");
  warning_files.emplace_back("file2.txt");
  // Null modal parent means the dialog is a system modal.
  EXPECT_CALL(
      *factory_,
      CreateWarnDialog(base::test::IsNotNullCallback(),
                       std::vector<DlpConfidentialFile>(
                           {warning_files.begin(), warning_files.end()}),
                       action, testing::IsNull(), testing::Eq(absl::nullopt)))
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
IN_PROC_BROWSER_TEST_P(OnDlpWarningNotificationClickedTest,
                       CancelShowsNoDialog) {
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
    OnDlpWarningNotificationClickedTest,
    ::testing::Values(
        std::make_tuple(dlp::FileAction::kUpload,
                        DlpFileDestination(kExampleUrl)),
        std::make_tuple(dlp::FileAction::kMove,
                        DlpFileDestination(data_controls::Component::kDrive))));

class OnDlpErrorNotificationClickedTest
    : public OnNotificationClickedTest,
      public ::testing::WithParamInterface<
          std::tuple<dlp::FileAction, DlpFileDestination>> {};

// Tests that clicking the OK button on an error notification for multiple-
// files shows a dialog.
IN_PROC_BROWSER_TEST_P(OnDlpErrorNotificationClickedTest,
                       MultiFileOKShowsDialog) {
  auto [action, destination] = GetParam();
  BlockedFilesMap blocked_map;
  blocked_map.emplace(base::FilePath("file1.txt"), Policy::kDlp);
  blocked_map.emplace(base::FilePath("file2.txt"), Policy::kDlp);
  EXPECT_CALL(*factory_,
              CreateErrorDialog(blocked_map, action, testing::NotNull()))
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

// Tests that clicking on the error notification, but no button is ignored.
IN_PROC_BROWSER_TEST_P(OnDlpErrorNotificationClickedTest,
                       MultiFileNoButtonIgnored) {
  auto [action, destination] = GetParam();
  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  std::vector<base::FilePath> blocked_files;
  blocked_files.emplace_back("file1.txt");
  blocked_files.emplace_back("file2.txt");
  fpnm->ShowDlpBlockedFiles(absl::nullopt, std::move(blocked_files), action);

  ASSERT_TRUE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
  bridge_->Click(kNotificationId, /*button_index=*/absl::nullopt);
  // The notification shouldn't be closed.
  EXPECT_TRUE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
}

// Tests that closing the error notification (e.g. by X or Dismiss all)
// correctly closes it.
IN_PROC_BROWSER_TEST_P(OnDlpErrorNotificationClickedTest,
                       MultiFileCloseCancels) {
  auto [action, destination] = GetParam();

  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  std::vector<base::FilePath> blocked_files;
  blocked_files.emplace_back("file1.txt");
  blocked_files.emplace_back("file2.txt");
  fpnm->ShowDlpBlockedFiles(absl::nullopt, std::move(blocked_files), action);

  auto notification = bridge_->GetDisplayedNotification(kNotificationId);
  ASSERT_TRUE(notification.has_value());
  notification->delegate()->Close(
      /*by_user=*/false);  // parameter doesn't matter
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId).has_value());
}

// Tests that clicking the OK button on an error notification for multiple
// files shows a system modal dialog when Files app doesn't launch before
// timeout.
IN_PROC_BROWSER_TEST_P(OnDlpErrorNotificationClickedTest,
                       MultiFileOKShowsDialog_Timeout) {
  auto [action, destination] = GetParam();
  BlockedFilesMap blocked_map;
  blocked_map.emplace(base::FilePath("file1.txt"), Policy::kDlp);
  blocked_map.emplace(base::FilePath("file2.txt"), Policy::kDlp);
  // Null modal parent means the dialog is a system modal.
  EXPECT_CALL(*factory_,
              CreateErrorDialog(blocked_map, action, testing::IsNull()))
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

// Tests that clicking the Cancel button on an error notification dismisses
// the notification without showing the dialog.
IN_PROC_BROWSER_TEST_P(OnDlpErrorNotificationClickedTest, CancelDismisses) {
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
    OnDlpErrorNotificationClickedTest,
    ::testing::Values(
        std::make_tuple(dlp::FileAction::kOpen,
                        DlpFileDestination(kExampleUrl)),
        std::make_tuple(dlp::FileAction::kDownload,
                        DlpFileDestination(data_controls::Component::kUsb))));

class IOTaskBrowserTest
    : public OnNotificationClickedTest,
      public ::testing::WithParamInterface<
          std::tuple<file_manager::io_task::OperationType, dlp::FileAction>> {
 protected:
  void SetUpOnMainThread() override {
    OnNotificationClickedTest::SetUpOnMainThread();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ = file_manager::util::GetFileManagerFileSystemContext(
        browser()->profile());
    // DLP Setup.
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&IOTaskBrowserTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
    ASSERT_NE(policy::DlpRulesManagerFactory::GetForPrimaryProfile()
                  ->GetDlpFilesController(),
              nullptr);
    fpnm_ = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
        browser()->profile());
    ASSERT_TRUE(fpnm_);
  }

  void TearDownOnMainThread() override {
    files_controller_.reset();
    OnNotificationClickedTest::TearDownOnMainThread();
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>();
    mock_rules_manager_ = dlp_rules_manager.get();
    ON_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));

    files_controller_ =
        std::make_unique<testing::NiceMock<policy::MockDlpFilesControllerAsh>>(
            *mock_rules_manager_);

    ON_CALL(*mock_rules_manager_, GetDlpFilesController())
        .WillByDefault(::testing::Return(files_controller_.get()));

    return dlp_rules_manager;
  }

  // Expects CheckIfTransferAllowed to be called. Once called, it calls
  // FilesPolicyNotificationManager to show a warning.
  void ExpectCheckIfTransferAllowedToWarn(
      const file_manager::io_task::IOTaskId task_id,
      const dlp::FileAction action,
      const bool expected_should_proceed,
      const std::vector<base::FilePath>& warning_files) {
    bool is_move = (action == dlp::FileAction::kMove) ? true : false;
    auto warn_on_check =
        [=](absl::optional<file_manager::io_task::IOTaskId> task_id,
            const std::vector<storage::FileSystemURL>& transferred_files,
            storage::FileSystemURL destination, bool is_move,
            DlpFilesControllerAsh::CheckIfTransferAllowedCallback
                result_callback) {
          auto warn_cb = base::BindOnce(
              [](DlpFilesControllerAsh::CheckIfTransferAllowedCallback cb,
                 const bool expected_should_proceed, bool should_proceed) {
                EXPECT_EQ(should_proceed, expected_should_proceed);
                // No file is blocked.
                std::move(cb).Run({});
              },
              std::move(result_callback), expected_should_proceed);
          fpnm_->ShowDlpWarning(std::move(warn_cb), task_id.value(),
                                warning_files, DlpFileDestination(""), action);
        };

    EXPECT_CALL(*files_controller_,
                CheckIfTransferAllowed(absl::make_optional(task_id), testing::_,
                                       testing::_, is_move, testing::_))
        .WillOnce(testing::Invoke(warn_on_check));
  }

  // Expects CheckIfTransferAllowed to be called. Once called, it calls
  // FilesPolicyNotificationManager to show the blocked files.
  void ExpectCheckIfTransferAllowedToBlock(
      const file_manager::io_task::IOTaskId task_id,
      const dlp::FileAction action,
      const std::vector<base::FilePath>& blocked_files) {
    bool is_move = (action == dlp::FileAction::kMove) ? true : false;
    auto block_on_check =
        [=](absl::optional<file_manager::io_task::IOTaskId> task_id,
            const std::vector<storage::FileSystemURL>& transferred_files,
            storage::FileSystemURL destination, bool is_move,
            DlpFilesControllerAsh::CheckIfTransferAllowedCallback
                result_callback) {
          fpnm_->ShowDlpBlockedFiles(task_id.value(), blocked_files, action);
          // Return transferred files as blocked.
          std::move(result_callback).Run(transferred_files);
        };

    EXPECT_CALL(*files_controller_,
                CheckIfTransferAllowed(absl::make_optional(task_id), testing::_,
                                       testing::_, is_move, testing::_))
        .WillOnce(testing::Invoke(block_on_check));
  }

  // Expects CheckIfTransferAllowed to be called. Once called, it calls
  // FilesPolicyNotificationManager to show a warning then calls it again to
  // show the blocked files.
  void ExpectCheckIfTransferAllowedToWarnAndBlock(
      const file_manager::io_task::IOTaskId task_id,
      const dlp::FileAction action,
      const bool expected_should_proceed,
      const std::vector<base::FilePath>& warning_files,
      const std::vector<base::FilePath>& blocked_files) {
    bool is_move = (action == dlp::FileAction::kMove) ? true : false;
    auto warn_on_check =
        [=](absl::optional<file_manager::io_task::IOTaskId> task_id,
            const std::vector<storage::FileSystemURL>& transferred_files,
            storage::FileSystemURL destination, bool is_move,
            DlpFilesControllerAsh::CheckIfTransferAllowedCallback
                result_callback) {
          auto warn_cb = base::BindOnce(
              [](DlpFilesControllerAsh::CheckIfTransferAllowedCallback cb,
                 const std::vector<storage::FileSystemURL>& transferred_files,
                 const bool expected_should_proceed, bool should_proceed) {
                EXPECT_EQ(should_proceed, expected_should_proceed);
                // Return transferred files as blocked.
                std::move(cb).Run(transferred_files);
              },
              std::move(result_callback), transferred_files,
              expected_should_proceed);
          fpnm_->ShowDlpWarning(std::move(warn_cb), task_id.value(),
                                warning_files, DlpFileDestination(""), action);
        };

    EXPECT_CALL(*files_controller_,
                CheckIfTransferAllowed(absl::make_optional(task_id), testing::_,
                                       testing::_, is_move, testing::_))
        .WillOnce(testing::Invoke(warn_on_check));
    fpnm_->ShowDlpBlockedFiles(task_id, blocked_files, action);
  }

  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://abc");

  raw_ptr<policy::MockDlpRulesManager, ExperimentalAsh> mock_rules_manager_ =
      nullptr;
  std::unique_ptr<policy::MockDlpFilesControllerAsh> files_controller_;
  raw_ptr<policy::FilesPolicyNotificationManager, ExperimentalAsh> fpnm_ =
      nullptr;
};

// Tests that clicking the OK button on a warning notification shown for copy or
// move IO task with multiple warning files shows a dialog instead of continuing
// the action, and opens the Files App only if there's not one opened already.
IN_PROC_BROWSER_TEST_P(IOTaskBrowserTest,
                       MultiFileOKShowsDialogOverFilesApp_Warning) {
  auto [type, action] = GetParam();

  // 2 dialogs should be shown.
  std::vector<base::FilePath> warning_files;
  warning_files.emplace_back("file1.txt");
  warning_files.emplace_back("file2.txt");
  EXPECT_CALL(
      *factory_,
      CreateWarnDialog(base::test::IsNotNullCallback(),
                       std::vector<DlpConfidentialFile>(
                           {warning_files.begin(), warning_files.end()}),
                       action, testing::NotNull(), testing::Eq(absl::nullopt)))
      .Times(2)
      .WillRepeatedly([](OnDlpRestrictionCheckedCallback callback,
                         std::vector<DlpConfidentialFile> files,
                         dlp::FileAction file_action,
                         gfx::NativeWindow modal_parent,
                         absl::optional<DlpFileDestination> destination) {
        // Cancel the task so it's deleted properly.
        std::move(callback).Run(/*should_proceed=*/false);
        return nullptr;
      });

  // No Files app opened.
  ASSERT_FALSE(FindFilesApp());

  // CheckIfTransferAllowed will call FPNM to show the warning which will pause
  // the IO task and trigger the notification. Do this before any Files App is
  // opened so that we are sure we show system notifications.
  ExpectCheckIfTransferAllowedToWarn(kTaskId1, action,
                                     /*expected_should_proceed=*/false,
                                     warning_files);
  ExpectCheckIfTransferAllowedToWarn(kTaskId2, action,
                                     /*expected_should_proceed=*/false,
                                     warning_files);

  // Add the tasks.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(policy::AddCopyOrMoveIOTask(
                     browser()->profile(), file_system_context_, kTaskId1, type,
                     temp_dir_.GetPath(), "test1.txt", kTestStorageKey)
                     .empty());
    ASSERT_FALSE(policy::AddCopyOrMoveIOTask(
                     browser()->profile(), file_system_context_, kTaskId2, type,
                     temp_dir_.GetPath(), "test2.txt", kTestStorageKey)
                     .empty());
  }

  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId1));
  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId2));

  auto notification = bridge_->GetDisplayedNotification(kNotificationId1);
  ASSERT_TRUE(notification.has_value());
  const std::u16string title =
      action == dlp::FileAction::kCopy
          ? l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_COPY_REVIEW_TITLE)
          : l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_MOVE_REVIEW_TITLE);

  notification = bridge_->GetDisplayedNotification(kNotificationId2);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), title);

  // Show the first dialog.
  ASSERT_TRUE(bridge_->GetDisplayedNotification(kNotificationId1).has_value());
  EXPECT_EQ(notification->title(), title);
  bridge_->Click(kNotificationId1, NotificationButton::OK);

  // Check that a new Files app is opened.
  Browser* first_app = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(first_app);
  ASSERT_EQ(first_app, FindFilesApp());

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId1).has_value());

  // Show the second dialog.
  ASSERT_TRUE(bridge_->GetDisplayedNotification(kNotificationId2).has_value());
  bridge_->Click(kNotificationId2, NotificationButton::OK);

  // Check that the last active Files app is the same as before.
  ASSERT_TRUE(first_app);
  ASSERT_EQ(first_app, FindFilesApp());

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId2).has_value());
}

// Tests that clicking the Cancel button on a warning notification shown for
// copy or move IO task with multiple warning files will cancel the task.
IN_PROC_BROWSER_TEST_P(IOTaskBrowserTest, MultiFileDismissCancels_Warning) {
  auto [type, action] = GetParam();

  // CheckIfTransferAllowed will call FPNM to show the warning which will pause
  // the IO task and trigger the notification.
  ExpectCheckIfTransferAllowedToWarn(
      kTaskId1, action,
      /*expected_should_proceed=*/false,
      {base::FilePath("file1.txt"), base::FilePath("file2.txt")});

  // Add the task.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(policy::AddCopyOrMoveIOTask(
                     browser()->profile(), file_system_context_, kTaskId1, type,
                     temp_dir_.GetPath(), "test1.txt", kTestStorageKey)
                     .empty());
  }

  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId1));

  auto notification = bridge_->GetDisplayedNotification(kNotificationId1);
  ASSERT_TRUE(notification.has_value());
  const std::u16string title =
      action == dlp::FileAction::kCopy
          ? l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_COPY_REVIEW_TITLE)
          : l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_MOVE_REVIEW_TITLE);

  // Cancel the warning.
  EXPECT_EQ(notification->title(), title);
  bridge_->Click(kNotificationId1, NotificationButton::CANCEL);

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId1).has_value());

  // Task info is removed when the task is cancelled.
  EXPECT_FALSE(fpnm_->HasIOTask(kTaskId1));
}

// Tests that clicking the OK button on a warning notification shown for
// copy or move IO task with single warning file will proceed the task.
IN_PROC_BROWSER_TEST_P(IOTaskBrowserTest, SingleFileOkProceeds_Warning) {
  auto [type, action] = GetParam();

  // CheckIfTransferAllowed will call FPNM to show the warning which will pause
  // the IO task and trigger the notification.
  ExpectCheckIfTransferAllowedToWarn(kTaskId1, action,
                                     /*expected_should_proceed=*/true,
                                     {base::FilePath("test1.txt")});

  // Add the task.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(policy::AddCopyOrMoveIOTask(
                     browser()->profile(), file_system_context_, kTaskId1, type,
                     temp_dir_.GetPath(), "test1.txt", kTestStorageKey)
                     .empty());
  }
  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId1));

  auto notification = bridge_->GetDisplayedNotification(kNotificationId1);
  ASSERT_TRUE(notification.has_value());
  const std::u16string title =
      action == dlp::FileAction::kCopy
          ? l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_COPY_REVIEW_TITLE)
          : l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_MOVE_REVIEW_TITLE);

  // Proceed the warning.
  EXPECT_EQ(notification->title(), title);
  bridge_->Click(kNotificationId1, NotificationButton::OK);

  // The warning notification should be closed or replaced by in progress one.
  notification = bridge_->GetDisplayedNotification(kNotificationId1);
  EXPECT_TRUE(!notification.has_value() || notification->title() != title);

  // Wait till IO task is complete.
  base::RunLoop().RunUntilIdle();

  // Task info should be cleared because there's not any blocked file.
  ASSERT_FALSE(fpnm_->HasIOTask(kTaskId1));
}

// Tests that clicking the OK button on an error notification shown for copy or
// move IO task with multiple blocked files shows a dialog, for which it opens
// the Files App only if there's not one opened already.
IN_PROC_BROWSER_TEST_P(IOTaskBrowserTest,
                       MultiFileOKShowsDialogOverFilesApp_Error) {
  auto [type, action] = GetParam();

  BlockedFilesMap blocked_map;
  blocked_map.emplace(base::FilePath("file1.txt"), Policy::kDlp);
  blocked_map.emplace(base::FilePath("file2.txt"), Policy::kDlp);
  EXPECT_CALL(*factory_,
              CreateErrorDialog(blocked_map, action, testing::NotNull()))
      .Times(2);

  // No Files app opened.
  ASSERT_FALSE(FindFilesApp());

  // CheckIfTransferAllowed will call FPNM to save the blocked files. Once we
  // complete the tasks with policy error, the file_manager::EventRouter will
  // notify FPNM with the error status and trigger the notification. Do this
  // before any Files App is opened so that we are sure we show system
  // notifications.
  ExpectCheckIfTransferAllowedToBlock(
      kTaskId1, action,
      {base::FilePath("file1.txt"), base::FilePath("file2.txt")});
  ExpectCheckIfTransferAllowedToBlock(
      kTaskId2, action,
      {base::FilePath("file1.txt"), base::FilePath("file2.txt")});

  // Add the tasks.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(policy::AddCopyOrMoveIOTask(
                     browser()->profile(), file_system_context_, kTaskId1, type,
                     temp_dir_.GetPath(), "test1.txt", kTestStorageKey)
                     .empty());
    ASSERT_FALSE(policy::AddCopyOrMoveIOTask(
                     browser()->profile(), file_system_context_, kTaskId2, type,
                     temp_dir_.GetPath(), "test2.txt", kTestStorageKey)
                     .empty());
  }
  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId1));
  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId2));

  // Wait till IO tasks are complete.
  base::RunLoop().RunUntilIdle();

  // Task Info shouldn't be removed after completion.
  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId1));
  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId2));

  auto notification = bridge_->GetDisplayedNotification(kNotificationId1);
  ASSERT_TRUE(notification.has_value());
  const std::u16string title =
      action == dlp::FileAction::kCopy ? u"Blocked copy" : u"Blocked move";
  EXPECT_EQ(notification->title(), title);

  notification = bridge_->GetDisplayedNotification(kNotificationId2);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), title);

  // Show the first dialog.
  bridge_->Click(kNotificationId1, NotificationButton::OK);

  // Check that a new Files app is opened.
  Browser* first_app = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(first_app);
  ASSERT_EQ(first_app, FindFilesApp());
  // Task info is removed after the dialog is shown.
  EXPECT_FALSE(fpnm_->HasIOTask(kTaskId1));

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId1).has_value());

  // Show the second dialog. No new app should be opened.
  bridge_->Click(kNotificationId2, NotificationButton::OK);

  // Check that the last active Files app is the same as before.
  ASSERT_TRUE(first_app);
  ASSERT_EQ(first_app, FindFilesApp());
  // Task info is removed after the dialog is shown.
  EXPECT_FALSE(fpnm_->HasIOTask(kTaskId2));
}

// Tests that the IO task info for copy or move with multiple blocked files will
// be removed upon clicking the DISMISS button on the error notification.
IN_PROC_BROWSER_TEST_P(IOTaskBrowserTest, MultiFileDismissRemovesIOInfo_Error) {
  auto [type, action] = GetParam();

  // CheckIfTransferAllowed will call FPNM to save the blocked files. Once we
  // complete the tasks with policy error, the file_manager::EventRouter will
  // notify FPNM with the error status and trigger the notification.
  ExpectCheckIfTransferAllowedToBlock(
      kTaskId1, action,
      {base::FilePath("file1.txt"), base::FilePath("file2.txt")});

  // Add the task.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(policy::AddCopyOrMoveIOTask(
                     browser()->profile(), file_system_context_, kTaskId1, type,
                     temp_dir_.GetPath(), "test1.txt", kTestStorageKey)
                     .empty());
  }
  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId1));

  // Wait till IO tasks are complete.
  base::RunLoop().RunUntilIdle();

  // Task Info shouldn't be removed after completion.
  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId1));

  auto notification = bridge_->GetDisplayedNotification(kNotificationId1);
  ASSERT_TRUE(notification.has_value());
  const std::u16string title =
      action == dlp::FileAction::kCopy ? u"Blocked copy" : u"Blocked move";
  EXPECT_EQ(notification->title(), title);

  // Dismiss the notification.
  bridge_->Click(kNotificationId1, NotificationButton::CANCEL);
  // Task info is removed after the notification is dismissed.
  EXPECT_FALSE(fpnm_->HasIOTask(kTaskId1));

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId1).has_value());
}

// Tests that the IO task info for copy or move with single blocked file will
// be removed upon the error notification is clicked.
IN_PROC_BROWSER_TEST_P(IOTaskBrowserTest,
                       SingleFileNotificationRemovesIOInfo_Error) {
  auto [type, action] = GetParam();

  // CheckIfTransferAllowed will call FPNM to save the blocked files. Once we
  // complete the tasks with policy error, the file_manager::EventRouter will
  // notify FPNM with the error status and trigger the notification.
  ExpectCheckIfTransferAllowedToBlock(kTaskId1, action,
                                      {base::FilePath("test1.txt")});

  // Add the task.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(policy::AddCopyOrMoveIOTask(
                     browser()->profile(), file_system_context_, kTaskId1, type,
                     temp_dir_.GetPath(), "test1.txt", kTestStorageKey)
                     .empty());
  }
  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId1));

  // Wait till IO task is complete.
  base::RunLoop().RunUntilIdle();

  // Task Info shouldn't be removed after completion.
  EXPECT_TRUE(fpnm_->HasIOTask(kTaskId1));

  auto notification = bridge_->GetDisplayedNotification(kNotificationId1);
  ASSERT_TRUE(notification.has_value());
  const std::u16string title =
      action == dlp::FileAction::kCopy ? u"Blocked copy" : u"Blocked move";
  EXPECT_EQ(notification->title(), title);

  // Click Learn more.
  bridge_->Click(kNotificationId1, NotificationButton::OK);
  // Task info is removed after the notification is clicked.
  EXPECT_FALSE(fpnm_->HasIOTask(kTaskId1));

  // The notification should be closed.
  EXPECT_FALSE(bridge_->GetDisplayedNotification(kNotificationId1).has_value());
}

// Tests that clicking the OK button on a warning notification shown for
// copy or move IO task with single warning file and a single blocked file will
// proceed the task but a block notification will appear in the end for the
// blocked file.
IN_PROC_BROWSER_TEST_P(IOTaskBrowserTest, SingleFileOkProceeds_Mix) {
  auto [type, action] = GetParam();

  // CheckIfTransferAllowed will call FPNM to show the warning which will pause
  // the IO task and trigger the notification.
  ExpectCheckIfTransferAllowedToWarnAndBlock(kTaskId1, action,
                                             /*expected_should_proceed=*/true,
                                             {base::FilePath("file1.txt")},
                                             {base::FilePath("file2.txt")});

  // Add the task.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(policy::AddCopyOrMoveIOTask(
                     browser()->profile(), file_system_context_, kTaskId1, type,
                     temp_dir_.GetPath(), "test1.txt", kTestStorageKey)
                     .empty());
  }
  ASSERT_TRUE(fpnm_->HasIOTask(kTaskId1));

  auto notification = bridge_->GetDisplayedNotification(kNotificationId1);
  ASSERT_TRUE(notification.has_value());
  const std::u16string title1 =
      action == dlp::FileAction::kCopy
          ? l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_COPY_REVIEW_TITLE)
          : l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_MOVE_REVIEW_TITLE);
  EXPECT_EQ(notification->title(), title1);

  // Proceed the warning.
  bridge_->Click(kNotificationId1, NotificationButton::OK);

  // The warning notification should be closed or replaced by in progress one.
  notification = bridge_->GetDisplayedNotification(kNotificationId1);
  EXPECT_TRUE(!notification.has_value() || notification->title() != title1);

  // Wait till IO task is complete.
  base::RunLoop().RunUntilIdle();

  // Task info should be cleared because there's one blocked file.
  EXPECT_TRUE(fpnm_->HasIOTask(kTaskId1));

  // Error notification.
  const std::u16string title2 =
      action == dlp::FileAction::kCopy ? u"Blocked copy" : u"Blocked move";
  notification = bridge_->GetDisplayedNotification(kNotificationId1);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), title2);
}

INSTANTIATE_TEST_SUITE_P(
    FPNM,
    IOTaskBrowserTest,
    ::testing::Values(
        std::make_tuple(file_manager::io_task::OperationType::kCopy,
                        dlp::FileAction::kCopy),
        std::make_tuple(file_manager::io_task::OperationType::kMove,
                        dlp::FileAction::kMove)));

}  // namespace policy
