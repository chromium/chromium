// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
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
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy {

namespace {

using testing::Field;

using policy::AddCopyOrMoveIOTask;
using policy::kNotificationId;

constexpr char kExampleUrl[] = "https://example1.com";

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
IN_PROC_BROWSER_TEST_P(OnDlpWarningNotificationClickedTest,
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
IN_PROC_BROWSER_TEST_P(OnDlpErrorNotificationClickedTest,
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

class IOTaskStatusObserver
    : public file_manager::io_task::IOTaskController::Observer {
 public:
  MOCK_METHOD(void,
              OnIOTaskStatus,
              (const file_manager::io_task::ProgressStatus&),
              (override));
};

class OnIONotificationClickedTest
    : public OnNotificationClickedTest,
      public ::testing::WithParamInterface<
          std::tuple<file_manager::io_task::OperationType, dlp::FileAction>> {
 public:
  void SetUpOnMainThread() override {
    OnNotificationClickedTest::SetUpOnMainThread();
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindLambdaForTesting([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<file_manager::VolumeManager>(
                  Profile::FromBrowserContext(context), nullptr, nullptr,
                  ash::disks::DiskMountManager::GetInstance(), nullptr,
                  file_manager::VolumeManager::GetMtpStorageInfoCallback()));
        }));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_system_context_ = file_manager::util::GetFileManagerFileSystemContext(
        browser()->profile());
    // DLP Setup.
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&OnIONotificationClickedTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
    ASSERT_NE(policy::DlpRulesManagerFactory::GetForPrimaryProfile()
                  ->GetDlpFilesController(),
              nullptr);
  }

 protected:
  void SetObserverExpectations(const file_manager::io_task::IOTaskId& task_id,
                               const std::string& notification_id) {
    // Task is queued.
    EXPECT_CALL(
        observer_,
        OnIOTaskStatus(AllOf(
            Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
            Field(&file_manager::io_task::ProgressStatus::state,
                  file_manager::io_task::State::kQueued))))
        .Times(testing::AtLeast(1));
    // Task is paused.
    EXPECT_CALL(
        observer_,
        OnIOTaskStatus(AllOf(
            Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
            Field(&file_manager::io_task::ProgressStatus::state,
                  file_manager::io_task::State::kPaused))))
        .Times(testing::AtLeast(1))
        .WillOnce([this, &notification_id](
                      const file_manager::io_task::ProgressStatus& status) {
          FilesPolicyNotificationManagerFactory::GetForBrowserContext(
              browser()->profile())
              ->ShowsFilesPolicyNotification(notification_id, status);
        })
        .WillRepeatedly(testing::Return());
    // Task is cancelled.
    EXPECT_CALL(
        observer_,
        OnIOTaskStatus(AllOf(
            Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
            Field(&file_manager::io_task::ProgressStatus::state,
                  file_manager::io_task::State::kCancelled))))
        .Times(testing::AtLeast(1));
  }

  IOTaskStatusObserver observer_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://abc");

 private:
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

  raw_ptr<policy::MockDlpRulesManager, ExperimentalAsh> mock_rules_manager_ =
      nullptr;
  std::unique_ptr<policy::MockDlpFilesControllerAsh> files_controller_;
};

// Tests that clicking the OK button on a warning notification shown for copy or
// move IO task with multiple warning files shows a dialog instead of continuing
// the action, and opens the Files App only if there's not one opened already.
IN_PROC_BROWSER_TEST_P(OnIONotificationClickedTest,
                       MultiFileOKShowsDialogOverFilesApp) {
  policy::GetIOTaskController(browser()->profile())->AddObserver(&observer_);

  auto [type, action] = GetParam();

  // 2 dialogs should be shown.
  std::vector<base::FilePath> warning_files;
  warning_files.emplace_back("file1.txt");
  warning_files.emplace_back("file2.txt");
  EXPECT_CALL(
      *factory_,
      CreateWarnDialog(
          base::test::IsNotNullCallback(),
          std::vector<DlpConfidentialFile>(
              {warning_files.begin(), warning_files.end()}),
          testing::_,  // destination is not important and will be removed.
          action, testing::NotNull()))
      .Times(2)
      .WillRepeatedly([](OnDlpRestrictionCheckedCallback callback,
                         std::vector<DlpConfidentialFile> files,
                         DlpFileDestination destination,
                         dlp::FileAction file_action,
                         gfx::NativeWindow modal_parent) {
        // Cancel the task so it's deleted properly.
        std::move(callback).Run(/*should_proceed=*/false);
        return nullptr;
      });
  base::MockCallback<OnDlpRestrictionCheckedCallback> cb;
  EXPECT_CALL(cb, Run(/*should_proceed=*/false)).Times(2);

  // No Files app opened.
  ASSERT_FALSE(FindFilesApp());

  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);

  // Add the first task and set expectations.
  file_manager::io_task::IOTaskId task_id_1 = 1u;
  const std::string notification_id_1 = "swa-file-operation-1";
  SetObserverExpectations(task_id_1, notification_id_1);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(policy::AddCopyOrMoveIOTask(
                     browser()->profile(), file_system_context_, task_id_1,
                     type, temp_dir_.GetPath(), "test1.txt", kTestStorageKey)
                     .empty());
  }
  ASSERT_TRUE(fpnm->HasIOTask(task_id_1));

  // This should pause the task, which would normally notify
  // file_manager::EventRouter with the status, that would also sent the update
  // to FPNM. In the test this is simulated with the mock observer.
  fpnm->ShowDlpWarning(cb.Get(), task_id_1, warning_files,
                       DlpFileDestination(""), action);
  ASSERT_TRUE(bridge_->GetDisplayedNotification(notification_id_1).has_value());
  bridge_->Click(notification_id_1, NotificationButton::OK);

  // Check that a new Files app is opened.
  Browser* first_app = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(first_app);
  ASSERT_EQ(first_app, FindFilesApp());

  // The notification should be closed.
  EXPECT_FALSE(
      bridge_->GetDisplayedNotification(notification_id_1).has_value());

  // Add another task and set expectations.
  file_manager::io_task::IOTaskId task_id_2 = 2u;
  const std::string notification_id_2 = "swa-file-operation-2";
  SetObserverExpectations(task_id_2, notification_id_2);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(policy::AddCopyOrMoveIOTask(
                     browser()->profile(), file_system_context_, task_id_2,
                     type, temp_dir_.GetPath(), "test2.txt", kTestStorageKey)
                     .empty());
  }
  ASSERT_TRUE(fpnm->HasIOTask(task_id_2));

  // Show another notification and dialog. No new app should be opened.
  fpnm->ShowDlpWarning(cb.Get(), task_id_2, warning_files,
                       DlpFileDestination(""), action);
  ASSERT_TRUE(bridge_->GetDisplayedNotification(notification_id_2).has_value());
  bridge_->Click(notification_id_2, NotificationButton::OK);

  // Check that the last active Files app is the same as before.
  ASSERT_TRUE(first_app);
  ASSERT_EQ(first_app, FindFilesApp());
  policy::GetIOTaskController(browser()->profile())->RemoveObserver(&observer_);
}

INSTANTIATE_TEST_SUITE_P(
    FPNM,
    OnIONotificationClickedTest,
    ::testing::Values(
        std::make_tuple(file_manager::io_task::OperationType::kCopy,
                        dlp::FileAction::kCopy),
        std::make_tuple(file_manager::io_task::OperationType::kMove,
                        dlp::FileAction::kMove)));

}  // namespace policy
