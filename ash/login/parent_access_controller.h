// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_PARENT_ACCESS_CONTROLLER_H_
#define ASH_LOGIN_PARENT_ACCESS_CONTROLLER_H_

#include "ash/login/ui/pin_request_view.h"
#include "ash/login/ui/pin_request_widget.h"
#include "ash/public/cpp/child_accounts/parent_access_controller.h"

namespace ash {

// ParentAccessController serves as a single point of access for PIN requests
// regarding parent access. It takes care of showing and hiding the PIN UI, as
// well as logging usage metrics.
class ASH_EXPORT ParentAccessController : PinRequestView::Delegate {
 public:
  // Actions that originated in parent access dialog. These values are persisted
  // to metrics. Entries should not be renumbered and numeric values should
  // never be reused.
  enum class UMAAction {
    kValidationSuccess = 0,
    kValidationError = 1,
    kCanceledByUser = 2,
    kGetHelp = 3,
    kMaxValue = kGetHelp,
  };

  // Context in which parent access code was used. These values are persisted to
  // metrics. Entries should not be reordered and numeric values should never be
  // reused.
  enum class UMAUsage {
    kTimeLimits = 0,
    kTimeChangeLoginScreen = 1,
    kTimeChangeInSession = 2,
    kTimezoneChange = 3,
    kAddUserLoginScreen = 4,
    kReauhLoginScreen = 5,
    kMaxValue = kReauhLoginScreen,
  };

  // Histogram to log actions that originated in parent access dialog.
  static constexpr char kUMAParentAccessCodeAction[] =
      "Supervision.ParentAccessCode.Action";

  // Histogram to log context in which parent access code was used.
  static constexpr char kUMAParentAccessCodeUsage[] =
      "Supervision.ParentAccessCode.Usage";

  ParentAccessController();
  ParentAccessController(const ParentAccessController&) = delete;
  ParentAccessController& operator=(const ParentAccessController&) = delete;
  ~ParentAccessController() override;

  // PinRequestView::Delegate interface.
  PinRequestView::SubmissionResult OnPinSubmitted(
      const std::string& pin) override;
  void OnBack() override;
  void OnHelp(gfx::NativeWindow parent_window) override;

  // Shows a standalone parent access dialog. If |child_account_id| is valid, it
  // validates the parent access code for that child only, when it is empty it
  // validates the code for any child signed in the device.
  // |on_exit_callback| is invoked when the back button is clicked or the
  // correct code is entered.
  // |action| contains information about why the parent
  // access view is necessary, it is used to modify the view appearance by
  // changing the title and description strings and background color.
  // The parent access widget is a modal and already contains a dimmer, however
  // when another modal is the parent of the widget, the dimmer will be placed
  // behind the two windows.
  // |extra_dimmer| will create an extra dimmer between the two.
  // |validation_time| is the time that will be used to validate the
  // code, if null the system's time will be used. Note: this is intended for
  // children only. If a non child account id is provided, the validation will
  // necessarily fail.
  // Returns whether opening the dialog was successful. Will fail if another PIN
  // dialog is already opened.
  bool ShowWidget(const AccountId& child_account_id,
                  PinRequest::OnPinRequestDone on_exit_callback,
                  SupervisedAction action,
                  bool extra_dimmer,
                  base::Time validation_time);

 private:
  AccountId account_id_;
  SupervisedAction action_ = SupervisedAction::kUnlockTimeLimits;
  base::Time validation_time_;

  base::WeakPtrFactory<ParentAccessController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_PARENT_ACCESS_CONTROLLER_H_
