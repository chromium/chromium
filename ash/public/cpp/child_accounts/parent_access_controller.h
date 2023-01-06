// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CHILD_ACCOUNTS_PARENT_ACCESS_CONTROLLER_H_
#define ASH_PUBLIC_CPP_CHILD_ACCOUNTS_PARENT_ACCESS_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

class AccountId;

namespace ash {

// Actions that might require parental approval.
enum class SupervisedAction {
  // Unlock a Chromebook that is locked due to a Time Limit policy.
  kUnlockTimeLimits,
  // When Chrome is unable to automatically verify if the OS time is correct
  // the user becomes able to manually change the clock. The entry points are
  // the settings page (in-session) and the tray bubble (out-session).
  kUpdateClock,
  // Change timezone from the settings page.
  kUpdateTimezone,
  // Add user flow.
  kAddUser,
  // Re-authentication flow.
  kReauth,
};

// The result of parent access code validation.
enum class ParentCodeValidationResult {
  // Parent code is valid.
  kValid,
  // Parent code is invalid. Can also happen if the configuration on the device
  // is outdated.
  kInvalid,
  // No matching parent access code configuration available on the device.
  kNoConfig,
  // Internal error of the system processing parent access code.
  kInternalError,
};

// ParentAccessController serves as a single point of access for PIN requests
// regarding parent access. It takes care of showing and hiding the PIN UI, as
// well as logging usage metrics.
class ASH_PUBLIC_EXPORT ParentAccessController {
 public:
  ParentAccessController();
  virtual ~ParentAccessController();

  // Get the instance of |ParentAccessController|.
  static ParentAccessController* Get();

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
  virtual bool ShowWidget(const AccountId& child_account_id,
                          base::OnceCallback<void(bool success)> callback,
                          SupervisedAction action,
                          bool extra_dimmer,
                          base::Time validation_time) = 0;
};
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CHILD_ACCOUNTS_PARENT_ACCESS_CONTROLLER_H_