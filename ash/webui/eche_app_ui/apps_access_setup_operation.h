// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_SETUP_OPERATION_H_
#define ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_SETUP_OPERATION_H_

#include <optional>
#include <ostream>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace ash {
namespace eche_app {

// Implements the apps access setup flow. This flow involves:
// (1) Creating a connection to the phone if one does not already exist.
// (2) Sending a message to the phone which asks it to begin the setup flow;
//     upon receipt of the message, the phone displays a UI which asks the
//     user to enable apps access for Phone Hub.
// (3) Waiting for the user to complete the flow; once the flow is complete, the
//     phone sends a message back to this device which indicates that
//     apps access has been granted.
class AppsAccessSetupOperation {
 public:
  // Note: Numerical values should not be changed because they must stay in
  // sync with multidevice_permissions_setup_dialog.js, with the
  // exception of NOT_STARTED, which has a value of 0. Also, these values are
  // persisted to logs. Entries should not be renumbered and numeric values
  // should never be reused. If entries are added, kMaxValue should be updated.
  enum class Status {
    // Connecting to the phone in order to set up apps access.
    kConnecting = 1,

    // No connection was able to be made to the phone within the expected time
    // period.
    kTimedOutConnecting = 2,

    // A connection to the phone was successful, but it unexpectedly became
    // disconnected before the setup flow could complete.
    kConnectionDisconnected = 3,

    // A connection to the phone has succeeded, and a message has been sent to
    // the phone to start the apps access opt-in flow. However, the user
    // has not yet completed the flow phone-side.
    kSentMessageToPhoneAndWaitingForResponse = 4,

    // The user has completed the phone-side opt-in flow.
    kCompletedSuccessfully = 5,

    // The user's phone is prohibited from granting feature access (e.g.,
    // the user could be using a Work Profile). Apps access will not have this
    // case.
    kProhibitedFromProvidingAccess = 6,

    // The user has rejected the phone-side opt-in flow.
    kCompletedUserRejected = 7,

    // The operation failed or cancelled because a system error.
    kOperationFailedOrCancelled = 8,

    kMaxValue = kOperationFailedOrCancelled
  };

  // Returns true if the provided status is the final one for this operation,
  // indicating either success or failure.
  static bool IsFinalStatus(Status status);

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when status of the setup flow has changed.
    virtual void OnAppsStatusChange(Status new_status) = 0;
  };

  AppsAccessSetupOperation(const AppsAccessSetupOperation&) = delete;
  AppsAccessSetupOperation& operator=(const AppsAccessSetupOperation&) = delete;
  virtual ~AppsAccessSetupOperation();

 private:
  friend class AppsAccessManager;
  friend class AppsAccessSetupOperationTest;

  AppsAccessSetupOperation(Delegate* delegate,
                           base::OnceClosure destructor_callback);

  void NotifyAppsStatusChanged(Status new_status);

  std::optional<Status> current_status_;
  const base::TimeTicks start_timestamp_ = base::TimeTicks::Now();
  const raw_ptr<Delegate> delegate_;
  base::OnceClosure destructor_callback_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_SETUP_OPERATION_H_
