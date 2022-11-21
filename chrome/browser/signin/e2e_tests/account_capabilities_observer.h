// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_E2E_TESTS_ACCOUNT_CAPABILITIES_OBSERVER_H_
#define CHROME_BROWSER_SIGNIN_E2E_TESTS_ACCOUNT_CAPABILITIES_OBSERVER_H_

#include "base/run_loop.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin::test {

// Observer class allowing to wait for account capabilities to be known.
class AccountCapabilitiesObserver : public IdentityManager::Observer {
 public:
  explicit AccountCapabilitiesObserver(IdentityManager* identity_manager);
  ~AccountCapabilitiesObserver() override;

  // IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // This should be called only once per AccountCapabilitiesObserver instance.
  void WaitForAllCapabilitiesToBeKnown(CoreAccountId account_id);

 private:
  raw_ptr<IdentityManager> identity_manager_ = nullptr;
  CoreAccountId account_id_;
  base::RunLoop run_loop_;
  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      identity_manager_observation_{this};
};

}  // namespace signin::test

#endif  // CHROME_BROWSER_SIGNIN_E2E_TESTS_ACCOUNT_CAPABILITIES_OBSERVER_H_
