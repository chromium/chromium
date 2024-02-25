// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_cloud_management_register_watcher.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_controller_desktop.h"
#include "chrome/browser/ui/enterprise_startup_dialog.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using RegisterResult =
    policy::ChromeBrowserCloudManagementController::RegisterResult;

namespace policy {

namespace {

constexpr char kEnrollmentToken[] = "enrollment-token";
constexpr char kDMToken[] = "dm-token";
constexpr char kClientId[] = "client-id";

// A fake ChromeBrowserCloudManagementController that notifies all observers the
// chrome browser cloud management enrollment process has been finished.
class FakeChromeBrowserCloudManagementController
    : public ChromeBrowserCloudManagementController {
 public:
  explicit FakeChromeBrowserCloudManagementController(
      std::unique_ptr<ChromeBrowserCloudManagementController::Delegate>
          delegate)
      : ChromeBrowserCloudManagementController(std::move(delegate)) {}
  FakeChromeBrowserCloudManagementController(
      const FakeChromeBrowserCloudManagementController&) = delete;
  FakeChromeBrowserCloudManagementController& operator=(
      const FakeChromeBrowserCloudManagementController&) = delete;

  void FireNotification(bool succeeded) {
    NotifyPolicyRegisterFinished(succeeded);
  }
};

// A mock EnterpriseStartDialog to mimic the behavior of real dialog.
class MockEnterpriseStartupDialog : public EnterpriseStartupDialog {
 public:
  MockEnterpriseStartupDialog() = default;
  MockEnterpriseStartupDialog(const MockEnterpriseStartupDialog&) = delete;
  MockEnterpriseStartupDialog& operator=(const MockEnterpriseStartupDialog&) =
      delete;
  ~MockEnterpriseStartupDialog() override {
    // |callback_| exists if we're mocking the process that dialog is dismissed
    // automatically.
    if (callback_) {
      std::move(callback_).Run(false /* was_accepted */,
                               true /* can_show_browser_window */);
    }
  }

  MOCK_METHOD1(DisplayLaunchingInformationWithThrobber,
               void(const std::u16string&));
  MOCK_METHOD2(DisplayErrorMessage,
               void(const std::u16string&,
                    const std::optional<std::u16string>&));
  MOCK_METHOD0(IsShowing, bool());

  void SetCallback(EnterpriseStartupDialog::DialogResultCallback callback) {
    callback_ = std::move(callback);
  }

  void UserClickedTheButton(bool confirmed) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), confirmed,
                                  false /* can_show_browser_window */));
  }

 private:
  DialogResultCallback callback_;
};

}  // namespace

class ChromeBrowserCloudManagementRegisterWatcherTest : public ::testing::Test {
 public:
  ChromeBrowserCloudManagementRegisterWatcherTest()
      : controller_(
            std::make_unique<ChromeBrowserCloudManagementControllerDesktop>()),
        watcher_(&controller_),
        dialog_(std::make_unique<MockEnterpriseStartupDialog>()),
        dialog_ptr_(dialog_.get()) {
    storage_.SetEnrollmentToken(kEnrollmentToken);
    storage_.SetDMToken(std::string());
    storage_.SetClientId(kClientId);
    watcher_.SetDialogCreationCallbackForTesting(
        base::BindOnce(&ChromeBrowserCloudManagementRegisterWatcherTest::
                           CreateEnterpriseStartupDialog,
                       base::Unretained(this)));
  }
  ChromeBrowserCloudManagementRegisterWatcherTest(
      const ChromeBrowserCloudManagementRegisterWatcherTest&) = delete;
  ChromeBrowserCloudManagementRegisterWatcherTest& operator=(
      const ChromeBrowserCloudManagementRegisterWatcherTest&) = delete;

 protected:
  FakeBrowserDMTokenStorage* storage() { return &storage_; }
  FakeChromeBrowserCloudManagementController* controller() {
    return &controller_;
  }
  ChromeBrowserCloudManagementRegisterWatcher* watcher() { return &watcher_; }
  MockEnterpriseStartupDialog* dialog() { return dialog_ptr_; }

  std::unique_ptr<EnterpriseStartupDialog> CreateEnterpriseStartupDialog(
      EnterpriseStartupDialog::DialogResultCallback callback) {
    dialog_->SetCallback(std::move(callback));
    return std::move(dialog_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  FakeChromeBrowserCloudManagementController controller_;
  ChromeBrowserCloudManagementRegisterWatcher watcher_;
  FakeBrowserDMTokenStorage storage_;
  std::unique_ptr<MockEnterpriseStartupDialog> dialog_;
  raw_ptr<MockEnterpriseStartupDialog, DanglingUntriaged> dialog_ptr_;
};

TEST_F(ChromeBrowserCloudManagementRegisterWatcherTest,
       NoEnrollmentNeededWithDMToken) {
  storage()->SetDMToken(kDMToken);
  EXPECT_EQ(RegisterResult::kEnrollmentSuccessBeforeDialogDisplayed,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
}

TEST_F(ChromeBrowserCloudManagementRegisterWatcherTest,
       NoEnrollmentNeededWithoutEnrollmentToken) {
  storage()->SetEnrollmentToken(std::string());
  storage()->SetDMToken(std::string());
  EXPECT_EQ(RegisterResult::kNoEnrollmentNeeded,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
}

TEST_F(ChromeBrowserCloudManagementRegisterWatcherTest, EnrollmentSucceed) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayLaunchingInformationWithThrobber(_));
  EXPECT_CALL(*dialog(), IsShowing()).WillOnce(Return(true));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeChromeBrowserCloudManagementController::FireNotification,
          base::Unretained(controller()), true));
  EXPECT_EQ(RegisterResult::kEnrollmentSuccess,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kClosedSuccess,
      1);
}

TEST_F(ChromeBrowserCloudManagementRegisterWatcherTest,
       EnrollmentSucceedWithNoErrorMessageSetup) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayLaunchingInformationWithThrobber(_));
  EXPECT_CALL(*dialog(), IsShowing()).WillOnce(Return(true));
  storage()->SetEnrollmentErrorOption(false);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeChromeBrowserCloudManagementController::FireNotification,
          base::Unretained(controller()), true));
  EXPECT_EQ(RegisterResult::kEnrollmentSuccess,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kClosedSuccess,
      1);
}

TEST_F(ChromeBrowserCloudManagementRegisterWatcherTest,
       EnrollmentFailedAndQuit) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayLaunchingInformationWithThrobber(_));
  EXPECT_CALL(*dialog(), DisplayErrorMessage(_, _))
      .WillOnce(
          InvokeWithoutArgs([this] { dialog()->UserClickedTheButton(false); }));
  EXPECT_CALL(*dialog(), IsShowing()).WillOnce(Return(true));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeChromeBrowserCloudManagementController::FireNotification,
          base::Unretained(controller()), false));
  EXPECT_EQ(RegisterResult::kQuitDueToFailure,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kClosedFail,
      1);
}

TEST_F(ChromeBrowserCloudManagementRegisterWatcherTest,
       EnrollmentFailedAndRestart) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayLaunchingInformationWithThrobber(_));
  EXPECT_CALL(*dialog(), DisplayErrorMessage(_, _))
      .WillOnce(
          InvokeWithoutArgs([this] { dialog()->UserClickedTheButton(true); }));
  EXPECT_CALL(*dialog(), IsShowing()).WillOnce(Return(true));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeChromeBrowserCloudManagementController::FireNotification,
          base::Unretained(controller()), false));
  EXPECT_EQ(RegisterResult::kRestartDueToFailure,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kClosedRelaunch,
      1);
}

TEST_F(ChromeBrowserCloudManagementRegisterWatcherTest,
       EnrollmentCanceledBeforeFinish) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayLaunchingInformationWithThrobber(_));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockEnterpriseStartupDialog::UserClickedTheButton,
                     base::Unretained(dialog()), false));
  EXPECT_EQ(RegisterResult::kQuitDueToFailure,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kClosedAbort,
      1);
}

TEST_F(ChromeBrowserCloudManagementRegisterWatcherTest,
       EnrollmentCanceledBeforeFinishWithNoErrorMessageSetup) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayLaunchingInformationWithThrobber(_));
  storage()->SetEnrollmentErrorOption(false);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockEnterpriseStartupDialog::UserClickedTheButton,
                     base::Unretained(dialog()), false));
  EXPECT_EQ(RegisterResult::kQuitDueToFailure,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kClosedAbort,
      1);
}

TEST_F(ChromeBrowserCloudManagementRegisterWatcherTest,
       EnrollmentFailedBeforeDialogDisplay) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayErrorMessage(_, _))
      .WillOnce(
          InvokeWithoutArgs([this] { dialog()->UserClickedTheButton(false); }));
  controller()->FireNotification(false);
  EXPECT_EQ(RegisterResult::kQuitDueToFailure,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kClosedFail,
      1);
}

TEST_F(ChromeBrowserCloudManagementRegisterWatcherTest,
       EnrollmentFailedWithoutErrorMessage) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayLaunchingInformationWithThrobber(_));
  EXPECT_CALL(*dialog(), IsShowing()).WillOnce(Return(true));
  storage()->SetEnrollmentErrorOption(false);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeChromeBrowserCloudManagementController::FireNotification,
          base::Unretained(controller()), false));
  EXPECT_EQ(RegisterResult::kEnrollmentFailedSilently,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      ChromeBrowserCloudManagementRegisterWatcher::EnrollmentStartupDialog::
          kClosedFailAndIgnore,
      1);
}

TEST_F(ChromeBrowserCloudManagementRegisterWatcherTest,
       EnrollmentFailedBeforeDialogDisplayWithoutErrorMessage) {
  base::HistogramTester histogram_tester;

  storage()->SetEnrollmentErrorOption(false);
  controller()->FireNotification(false);
  EXPECT_EQ(RegisterResult::kEnrollmentFailedSilentlyBeforeDialogDisplayed,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectTotalCount(
      ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName,
      0);
}

}  // namespace policy
