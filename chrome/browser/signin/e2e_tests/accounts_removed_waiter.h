// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_E2E_TESTS_ACCOUNTS_REMOVED_WAITER_H_
#define CHROME_BROWSER_SIGNIN_E2E_TESTS_ACCOUNTS_REMOVED_WAITER_H_

#include "base/run_loop.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin::test {

// Helper class to wait until all accounts have been removed from the
// `IdentityManager`.
class AccountsRemovedWaiter : public signin::IdentityManager::Observer {
 public:
  explicit AccountsRemovedWaiter(signin::IdentityManager* identity_manager);
  ~AccountsRemovedWaiter() override;

  void Wait();

 private:
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;

  base::RunLoop run_loop_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      observation_{this};
};

}  // namespace signin::test

#endif  // CHROME_BROWSER_SIGNIN_E2E_TESTS_ACCOUNTS_REMOVED_WAITER_H_
