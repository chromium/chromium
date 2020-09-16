// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CONTROLLER_IMPL_H_
#define ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CONTROLLER_IMPL_H_

#include "ash/login/ui/pin_request_view.h"
#include "ash/login/ui/pin_request_widget.h"
#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"

namespace ash {

// Implementation of ParentAccessController. It serves as a single point of
// access for PIN requests regarding parent access. It takes care of showing and
// hiding the PIN UI, as well as logging usage metrics.
class ASH_EXPORT ParentAccessControllerImpl : public ParentAccessController,
                                              public PinRequestView::Delegate {
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

  ParentAccessControllerImpl();
  ParentAccessControllerImpl(const ParentAccessControllerImpl&) = delete;
  ParentAccessControllerImpl& operator=(const ParentAccessControllerImpl&) =
      delete;
  ~ParentAccessControllerImpl() override;

  // PinRequestView::Delegate:
  PinRequestView::SubmissionResult OnPinSubmitted(
      const std::string& pin) override;
  void OnBack() override;
  void OnHelp(gfx::NativeWindow parent_window) override;

  // ParentAccessController:
  bool ShowWidget(const AccountId& child_account_id,
                  PinRequest::OnPinRequestDone on_exit_callback,
                  SupervisedAction action,
                  bool extra_dimmer,
                  base::Time validation_time) override;

 private:
  AccountId account_id_;
  SupervisedAction action_ = SupervisedAction::kUnlockTimeLimits;
  base::Time validation_time_;

  base::WeakPtrFactory<ParentAccessControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CONTROLLER_IMPL_H_
