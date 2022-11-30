// Copyright 2020 The Chromium Authors
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
constexpr std::array<AppsAccessSetupOperation::Status, 5>
    kOperationFinishedStatus{
        AppsAccessSetupOperation::Status::kTimedOutConnecting,
        AppsAccessSetupOperation::Status::kConnectionDisconnected,
        AppsAccessSetupOperation::Status::kCompletedSuccessfully,
        AppsAccessSetupOperation::Status::kCompletedUserRejected,
        AppsAccessSetupOperation::Status::kOperationFailedOrCancelled,
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

}  // namespace eche_app
}  // namespace ash
