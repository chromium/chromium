// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_CHROMEOS_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_CHROMEOS_UTIL_H_

#include "components/account_manager_core/account_manager_facade.h"
#include "components/signin/public/base/signin_metrics.h"

namespace signin_ui_util {

// Converts `access_point` to a corresponding `AccountAdditionSource` for adding
// a new account.
account_manager::AccountManagerFacade::AccountAdditionSource
GetAddAccountSourceFromAccessPoint(signin_metrics::AccessPoint access_point);

// Converts `access_point` to a corresponding `AccountAdditionSource` for
// reauthenticating an existing new account.
account_manager::AccountManagerFacade::AccountAdditionSource
GetAccountReauthSourceFromAccessPoint(signin_metrics::AccessPoint access_point);

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_CHROMEOS_UTIL_H_
