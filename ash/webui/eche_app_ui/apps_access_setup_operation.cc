// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/apps_access_setup_operation.h"

#include <array>

#include "base/check.h"
#include "base/containers/contains.h"

namespace ash {
namespace eche_app {
namespace {

// Status values which are considered "final" - i.e., once the status of an
// operation changes to one of these values, the operation has completed. These
// status values indicate either a success or a fatal error.
constexpr std::array<AppsAccessSetupOperation::Status, 3>
    kOperationFinishedStatus{
        AppsAccessSetupOperation::Status::kTimedOutConnecting,
        AppsAccessSetupOperation::Status::kConnectionDisconnected,
        AppsAccessSetupOperation::Status::kCompletedSuccessfully,
    };
}  // namespace */

// static
bool AppsAccessSetupOperation::IsFinalStatus(Status status) {
  return base::Contains(kOperationFinishedStatus, status);
}

AppsAccessSetupOperation::AppsAccessSetupOperation(
    Delegate* delegate,
    base::OnceClosure destructor_callback)
    : delegate_(delegate),
      destructor_callback_(std::move(destructor_callback)) {
  DCHECK(delegate_);
  DCHECK(destructor_callback_);
}

AppsAccessSetupOperation::~AppsAccessSetupOperation() {
  std::move(destructor_callback_).Run();
}

void AppsAccessSetupOperation::NotifyAppsStatusChanged(Status new_status) {
  current_status_ = new_status;

  delegate_->OnAppsStatusChange(new_status);
}

std::ostream& operator<<(std::ostream& stream,
                         AppsAccessSetupOperation::Status status) {
  switch (status) {
    case AppsAccessSetupOperation::Status::kConnecting:
      stream << "[Connecting]";
      break;
    case AppsAccessSetupOperation::Status::kTimedOutConnecting:
      stream << "[Timed out connecting]";
      break;
    case AppsAccessSetupOperation::Status::kConnectionDisconnected:
      stream << "[Connection disconnected]";
      break;
    case AppsAccessSetupOperation::Status::
        kSentMessageToPhoneAndWaitingForResponse:
      stream << "[Sent message to phone; waiting for response]";
      break;
    case AppsAccessSetupOperation::Status::kCompletedSuccessfully:
      stream << "[Completed successfully]";
      break;
  }

  return stream;
}

}  // namespace eche_app
}  // namespace ash
