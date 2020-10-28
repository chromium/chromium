// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCOUNT_MANAGER_FACADE_FACTORY_H_
#define CHROME_BROWSER_ACCOUNT_MANAGER_FACADE_FACTORY_H_

#include <string>

namespace account_manager {
class AccountManagerFacade;
}  // namespace account_manager

// A factory function for getting platform specific implementations of
// |AccountManagerFacade|.
// Returns the |AccountManagerFacade| for the given |profile_path|.
// Note that |AccountManagerFacade| is independent of a |Profile|, and this is
// needed only because of Multi-Login on Chrome OS, and will be removed soon.
account_manager::AccountManagerFacade* GetAccountManagerFacade(
    const std::string& profile_path);

#endif  // CHROME_BROWSER_ACCOUNT_MANAGER_FACADE_FACTORY_H_
