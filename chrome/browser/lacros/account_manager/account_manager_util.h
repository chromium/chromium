// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UTIL_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UTIL_H_

#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"

// Lists accounts that are available as secondary accounts for profile with
// `profile_path`. This passes back all accounts in the OS, excluding the
// accounts that are already present in the given profile. The accounts are
// returned in an arbitrary order. If the profile with `profile_path` contains
// no accounts or does not exist, it returns all accounts in the OS. The only
// async part of this function is retrieving accounts from `mapper`, i.e.
// `callback` gets called iff `mapper` does not get deleted before completion of
// the task.
void GetAllAvailableAccounts(
    AccountProfileMapper* mapper,
    const base::FilePath& profile_path,
    AccountProfileMapper::ListAccountsCallback callback);

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UTIL_H_
