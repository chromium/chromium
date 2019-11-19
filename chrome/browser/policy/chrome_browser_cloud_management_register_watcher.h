// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_REGISTER_WATCHER_H_
#define CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_REGISTER_WATCHER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_controller.h"
#include "chrome/browser/ui/enterprise_startup_dialog.h"

class ChromeBrowserCloudManagementRegisterWatcherTest;

namespace policy {

// Watches the status of chrome browser cloud management enrollment.
// Shows the blocking dialog for ongoing enrollment and failed enrollment.
class ChromeBrowserCloudManagementRegisterWatcher
    : public ChromeBrowserCloudManagementController::Observer {
 public:
  using DialogCreationCallback =
      base::OnceCallback<std::unique_ptr<EnterpriseStartupDialog>(
          EnterpriseStartupDialog::DialogResultCallback)>;

  explicit ChromeBrowserCloudManagementRegisterWatcher(
      ChromeBrowserCloudManagementController* controller);
  ~ChromeBrowserCloudManagementRegisterWatcher() override;

  // Blocks until the  chrome browser cloud management enrollment process
  // finishes. Returns the result of enrollment.
  ChromeBrowserCloudManagementController::RegisterResult
  WaitUntilCloudPolicyEnrollmentFinished();

  // Returns whether the dialog is being displayed.
  bool IsDialogShowing();

  void SetDialogCreationCallbackForTesting(DialogCreationCallback callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowserCloudManagementRegisterWatcherTest,
                           EnrollmentSucceed);
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowserCloudManagementRegisterWatcherTest,
                           EnrollmentSucceedWithNoErrorMessageSetup);
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowserCloudManagementRegisterWatcherTest,
                           EnrollmentFailedAndQuit);
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowserCloudManagementRegisterWatcherTest,
                           EnrollmentFailedAndRestart);
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowserCloudManagementRegisterWatcherTest,
                           EnrollmentCanceledBeforeFinish);
  FRIEND_TEST_ALL_PREFIXES(
      ChromeBrowserCloudManagementRegisterWatcherTest,
      EnrollmentCanceledBeforeFinishWithNoErrorMessageSetup);
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowserCloudManagementRegisterWatcherTest,
                           EnrollmentFailedBeforeDialogDisplay);
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowserCloudManagementRegisterWatcherTest,
                           EnrollmentFailedWithoutErrorMessage);
  FRIEND_TEST_ALL_PREFIXES(
      ChromeBrowserCloudManagementRegisterWatcherTest,
      EnrollmentFailedBeforeDialogDisplayWithoutErrorMessage);

  // Enum used with kStartupDialogHistogramName.
  enum class EnrollmentStartupDialog {
    // The enrollment startup dialog was shown.
    kShown = 0,

    // The dialog was closed automatically because enrollment completed
    // successfully.  Chrome startup can continue normally.
    kClosedSuccess = 1,

    // The dialog was closed because enrollment failed.  The user chose to
    // relaunch chrome and try again.
    kClosedRelaunch = 2,

    // The dialog was closed because enrollment failed.  The user chose to
    // close chrome.
    kClosedFail = 3,

    // The dialog was closed because no response from the server was received
    // before the user gave up and closed the dialog.
    kClosedAbort = 4,

    // The dialog was closed automatically because enrollment failed but admin
    // choose to ignore the error and show the browser window.
    kClosedFailAndIgnore = 5,

    kMaxValue = kClosedFailAndIgnore,
  };

  static const char kStartupDialogHistogramName[];

  static void RecordEnrollmentStartDialog(
      EnrollmentStartupDialog dialog_startup);

  // ChromeBrowserCloudManagementController::Observer
  void OnPolicyRegisterFinished(bool succeeded) override;

  // EnterpriseStartupDialog callback.
  void OnDialogClosed(bool is_accepted, bool can_show_browser_window);

  void DisplayErrorMessage();

  ChromeBrowserCloudManagementController* controller_;

  base::RunLoop run_loop_;
  std::unique_ptr<EnterpriseStartupDialog> dialog_;

  bool is_restart_needed_ = false;
  base::Optional<bool> register_result_;

  DialogCreationCallback dialog_creation_callback_;

  base::Time visible_start_time_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserCloudManagementRegisterWatcher);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_REGISTER_WATCHER_H_
