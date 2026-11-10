// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_E2E_TESTS_ACCOUNT_CAPABILITIES_OBSERVER_H_
#define CHROME_BROWSER_SIGNIN_E2E_TESTS_ACCOUNT_CAPABILITIES_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"

class AccountCapabilities;

namespace signin::test {

// Observer class allowing to wait for account capabilities to be fetched and
// verify that all supported capabilities are known.
class AccountCapabilitiesObserver : public IdentityManager::Observer {
 public:
  explicit AccountCapabilitiesObserver(IdentityManager* identity_manager);
  ~AccountCapabilitiesObserver() override;

  // IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // Waits until capabilities are fetched (`AreAnyCapabilitiesKnown()`) for
  // `account_id`, then expects that every capability in
  // `AccountCapabilities::GetSupportedAccountCapabilityNames()` is known,
  // logging `AccountCapabilities::GetCapabilityDisplayName()` for any
  // capability that remained `signin::Tribool::kUnknown`.
  // This should be called only once per `AccountCapabilitiesObserver` instance.
  void WaitForCapabilitiesFetchAndExpectAllKnown(CoreAccountId account_id);

 private:
  static void ExpectAllCapabilitiesKnown(
      const AccountCapabilities& capabilities);

  raw_ptr<IdentityManager> identity_manager_ = nullptr;
  CoreAccountId account_id_;
  base::test::TestFuture<void> capabilities_known_future_;
  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      identity_manager_observation_{this};
};

}  // namespace signin::test

#endif  // CHROME_BROWSER_SIGNIN_E2E_TESTS_ACCOUNT_CAPABILITIES_OBSERVER_H_
