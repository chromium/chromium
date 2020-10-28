// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/account_manager_facade_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/lacros/account_manager_facade_lacros.h"
#include "components/account_manager_core/account_manager_facade.h"

account_manager::AccountManagerFacade* GetAccountManagerFacade(
    const std::string& profile_path) {
  // Multi-Login is disabled with Lacros. Always return the same instance.
  static base::NoDestructor<AccountManagerFacadeLacros> facade;
  return facade.get();
}
