// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_id_from_account_info.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/known_user.h"
#endif

AccountId AccountIdFromAccountInfo(const CoreAccountInfo& account_info) {
#if defined(OS_CHROMEOS)
  return user_manager::known_user::GetAccountId(
      account_info.email, account_info.gaia, AccountType::GOOGLE);
#else
  if (account_info.email.empty() || account_info.gaia.empty())
    return EmptyAccountId();

  return AccountId::FromUserEmailGaiaId(
      gaia::CanonicalizeEmail(account_info.email), account_info.gaia);
#endif
}
