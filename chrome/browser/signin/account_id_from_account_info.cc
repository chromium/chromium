// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_id_from_account_info.h"
#include "build/chromeos_buildflags.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"

AccountId AccountIdFromAccountInfo(const CoreAccountInfo& account_info) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return AccountId::FromNonCanonicalEmail(account_info.email, account_info.gaia,
                                          AccountType::GOOGLE);
#else
  if (account_info.email.empty() || account_info.gaia.empty())
    return EmptyAccountId();

  return AccountId::FromUserEmailGaiaId(
      gaia::CanonicalizeEmail(account_info.email), account_info.gaia);
#endif
}
