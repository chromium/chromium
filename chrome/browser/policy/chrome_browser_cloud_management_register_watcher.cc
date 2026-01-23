// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_cloud_management_register_watcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/syslog_logging.h"
#include "chrome/grit/branded_strings.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

using RegisterResult = ChromeBrowserCloudManagementController::RegisterResult;

const char
    ChromeBrowserCloudManagementRegisterWatcher::kStartupDialogHistogramName[] =
        "Enterprise.MachineLevelUserCloudPolicyEnrollment.StartupDialog";

ChromeBrowserCloudManagementRegisterWatcher::
    ChromeBrowserCloudManagementRegisterWatcher(
        ChromeBrowserCloudManagementController* controller)
    : controller_(controller) {
  controller_->AddObserver(this);
}
ChromeBrowserCloudManagementRegisterWatcher::
    ~ChromeBrowserCloudManagementRegisterWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  controller_->RemoveObserver(this);
}

RegisterResult ChromeBrowserCloudManagementRegisterWatcher::
    WaitUntilCloudPolicyEnrollmentFinished() {
  BrowserDMTokenStorage* token_storage = BrowserDMTokenStorage::Get();

  if (token_storage->RetrieveEnrollmentToken().empty())
    return RegisterResult::kNoEnrollmentNeeded;

  // We are already enrolled successfully.
  if (token_storage->RetrieveDMToken().is_valid())
    return RegisterResult::kEnrollmentSuccessBeforeDialogDisplayed;

  bool registration_succeeded = true;
  if (register_result_.has_value()) {
    // |register_result_| has been set only if the enrollment has finished.
    // And it must be failed if it's finished without a DM token which is
    // checked above. Show the error message directly.
    DCHECK(!register_result_.value());

    if (!token_storage->ShouldDisplayErrorMessageOnFailure()) {
      return RegisterResult::kEnrollmentFailedSilentlyBeforeDialogDisplayed;
    }
    registration_succeeded = false;
  }

  // Unretained(this) is safe because `run_loop_` runs in the current scope
  // and is not quit until after `callback` executes.
  EnterpriseStartupDialog::DialogResultCallback callback = base::BindOnce(
      &ChromeBrowserCloudManagementRegisterWatcher::OnDialogClosed,
      base::Unretained(this));
  // Make sure the dialog is closed before returning from this function, to
  // enforce that `callback` cannot outlive this.
  base::ScopedClosureRunner dialog_resetter(base::BindOnce(
      [](std::unique_ptr<EnterpriseStartupDialog>& dialog) { dialog.reset(); },
      std::ref(dialog_)));

  if (test_create_dialog_callback_) {
    dialog_ = std::move(test_create_dialog_callback_).Run(std::move(callback));
  } else {
    dialog_ = EnterpriseStartupDialog::CreateAndShowDialog(std::move(callback));
  }

  visible_start_time_ = base::Time::Now();

  if (registration_succeeded) {
    // Display the loading dialog and wait for the enrollment process.
    dialog_->DisplayLaunchingInformationWithThrobber(l10n_util::GetStringUTF16(
        IDS_ENTERPRISE_STARTUP_CLOUD_POLICY_ENROLLMENT_TOOLTIP));

  } else {
    DisplayErrorMessage();
  }
  RecordEnrollmentStartDialog(EnrollmentStartupDialog::kShown);
  run_loop_.Run();
  if (register_result_.value_or(false))
    return RegisterResult::kEnrollmentSuccess;

  if (!token_storage->ShouldDisplayErrorMessageOnFailure() &&
      register_result_.has_value()) {
    SYSLOG(ERROR) << "Chrome browser cloud management enrollment has failed.";
    return RegisterResult::kEnrollmentFailedSilently;
  }

  SYSLOG(ERROR) << "Can not start Chrome as chrome browser cloud management "
                   "enrollment has failed. Please double check network "
                   "connection and the status of enrollment token then open "
                   "Chrome again.";

  if (is_restart_needed_)
    return RegisterResult::kRestartDueToFailure;
  return RegisterResult::kQuitDueToFailure;
}

bool ChromeBrowserCloudManagementRegisterWatcher::IsDialogShowing() {
  return (dialog_ && dialog_->IsShowing()) || run_loop_.running();
}

void ChromeBrowserCloudManagementRegisterWatcher::
    SetDialogCreationCallbackForTesting(DialogCreationCallback callback) {
  test_create_dialog_callback_ = std::move(callback);
}

// static
void ChromeBrowserCloudManagementRegisterWatcher::RecordEnrollmentStartDialog(
    EnrollmentStartupDialog dialog_startup) {
  UMA_HISTOGRAM_ENUMERATION(kStartupDialogHistogramName, dialog_startup);
}

void ChromeBrowserCloudManagementRegisterWatcher::OnPolicyRegisterFinished(
    bool succeeded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  register_result_ = succeeded;

  // If dialog still exists, dismiss the dialog for a success enrollment or
  // show the error message. If dialog has been closed before enrollment
  // finished, Chrome should already be in the shutdown process.
  if (dialog_ && dialog_->IsShowing()) {
    if (register_result_.value() ||
        !BrowserDMTokenStorage::Get()->ShouldDisplayErrorMessageOnFailure()) {
      dialog_.reset();
    } else {
      DisplayErrorMessage();
    }
  }
}

void ChromeBrowserCloudManagementRegisterWatcher::OnDialogClosed(
    bool is_accepted,
    bool can_show_browser_window) {
  if (can_show_browser_window) {
    // Chrome startup can continue normally.
    if (register_result_.value()) {
      RecordEnrollmentStartDialog(EnrollmentStartupDialog::kClosedSuccess);
    } else {
      RecordEnrollmentStartDialog(
          EnrollmentStartupDialog::kClosedFailAndIgnore);
    }
  } else if (is_accepted) {
    // User chose to restart chrome and try re-enrolling.
    RecordEnrollmentStartDialog(EnrollmentStartupDialog::kClosedRelaunch);
  } else if (register_result_.has_value()) {
    // User closed the dialog after seeing a message that enrollment failed.
    RecordEnrollmentStartDialog(EnrollmentStartupDialog::kClosedFail);
  } else {
    // User closed the dialog after waiting too long with no result.
    RecordEnrollmentStartDialog(EnrollmentStartupDialog::kClosedAbort);
  }

  base::TimeDelta visible_time = base::Time::Now() - visible_start_time_;
  UMA_HISTOGRAM_TIMES(
      "Enterprise.MachineLevelUserCloudPolicyEnrollment.StartupDialogTime",
      visible_time);

  // User confirm the dialog to relaunch Chrome to retry the register.
  is_restart_needed_ = is_accepted;

  // Resume the launch process once the dialog is closed.
  run_loop_.Quit();
}

void ChromeBrowserCloudManagementRegisterWatcher::DisplayErrorMessage() {
  dialog_->DisplayErrorMessage(
      l10n_util::GetStringUTF16(
          IDS_ENTERPRISE_STARTUP_CLOUD_POLICY_ENROLLMENT_ERROR),
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_STARTUP_RELAUNCH_BUTTON));
}

}  // namespace policy
