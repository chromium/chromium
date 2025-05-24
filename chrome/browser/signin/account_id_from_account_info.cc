// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_id_from_account_info.h"

#include "build/build_config.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "components/user_manager/known_user.h"
#endif

AccountId AccountIdFromAccountInfo(const CoreAccountInfo& account_info) {
#if BUILDFLAG(IS_CHROMEOS)
  user_manager::KnownUser known_user(g_browser_process->local_state());
  return known_user.GetAccountId(
      account_info.email, account_info.gaia.ToString(), AccountType::GOOGLE);
#else
  if (account_info.email.empty() || account_info.gaia.empty())
    return EmptyAccountId();

  return AccountId::FromUserEmailGaiaId(
      gaia::CanonicalizeEmail(account_info.email), account_info.gaia);
#endif
}
