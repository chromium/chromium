// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_FACADE_LACROS_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_FACADE_LACROS_H_

#include "chromeos/components/account_manager/account_manager_facade.h"

// Lacros specific implementation of |AccountManagerFacade| that talks to
// |chromeos::AccountManager|, residing in ash-chrome, over Mojo.
class AccountManagerFacadeLacros : public AccountManagerFacade {
 public:
  AccountManagerFacadeLacros();
  AccountManagerFacadeLacros(const AccountManagerFacadeLacros&) = delete;
  AccountManagerFacadeLacros& operator=(const AccountManagerFacadeLacros&) =
      delete;
  ~AccountManagerFacadeLacros() override;

  // AccountManagerFacade overrides:
  bool IsInitialized() override;
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_FACADE_LACROS_H_
