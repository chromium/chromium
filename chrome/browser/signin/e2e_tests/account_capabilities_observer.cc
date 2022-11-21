// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "account_capabilities_observer.h"

namespace signin::test {

AccountCapabilitiesObserver::AccountCapabilitiesObserver(
    IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  identity_manager_observation_.Observe(identity_manager);
}

AccountCapabilitiesObserver::~AccountCapabilitiesObserver() = default;

void AccountCapabilitiesObserver::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.account_id != account_id_)
    return;

  if (info.capabilities.AreAllCapabilitiesKnown())
    run_loop_.Quit();
}

// This should be called only once per AccountCapabilitiesObserver instance.
void AccountCapabilitiesObserver::WaitForAllCapabilitiesToBeKnown(
    CoreAccountId account_id) {
  DCHECK(
      identity_manager_observation_.IsObservingSource(identity_manager_.get()));
  AccountInfo info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  if (info.capabilities.AreAllCapabilitiesKnown())
    return;

  account_id_ = account_id;
  run_loop_.Run();
  identity_manager_observation_.Reset();
}

}  // namespace signin::test
