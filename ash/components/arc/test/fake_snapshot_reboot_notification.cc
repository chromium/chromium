// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_snapshot_reboot_notification.h"

namespace arc {
namespace data_snapshotd {

FakeSnapshotRebootNotification::FakeSnapshotRebootNotification() = default;
FakeSnapshotRebootNotification::~FakeSnapshotRebootNotification() = default;

void FakeSnapshotRebootNotification::SetUserConsentCallback(
    const base::RepeatingClosure& closure) {
  user_consent_callback_ = closure;
}

void FakeSnapshotRebootNotification::Show() {
  shown_ = true;
}

void FakeSnapshotRebootNotification::Hide() {
  shown_ = false;
}

void FakeSnapshotRebootNotification::Click() {
  user_consent_callback_.Run();
}

}  // namespace data_snapshotd
}  // namespace arc
