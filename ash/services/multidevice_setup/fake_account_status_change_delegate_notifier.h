// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_
#define ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_

#include "ash/services/multidevice_setup/account_status_change_delegate_notifier.h"

namespace ash {

namespace multidevice_setup {

// Test AccountStatusChangeDelegateNotifier implementation.
class FakeAccountStatusChangeDelegateNotifier
    : public AccountStatusChangeDelegateNotifier {
 public:
  FakeAccountStatusChangeDelegateNotifier() = default;

  FakeAccountStatusChangeDelegateNotifier(
      const FakeAccountStatusChangeDelegateNotifier&) = delete;
  FakeAccountStatusChangeDelegateNotifier& operator=(
      const FakeAccountStatusChangeDelegateNotifier&) = delete;

  ~FakeAccountStatusChangeDelegateNotifier() override = default;

  using AccountStatusChangeDelegateNotifier::delegate;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_
