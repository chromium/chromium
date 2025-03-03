// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"

namespace signin_ui_util {

void SigninUiDelegate::ShowTurnSyncOnUI(
    Profile* profile,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const CoreAccountId& account_id,
    TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode,
    bool is_sync_promo,
    bool turn_sync_on_signed_profile) {
  // TurnSyncOnHelper is suicidal (it will delete itself once it finishes
  // enabling sync).
  new TurnSyncOnHelper(profile, EnsureBrowser(profile), access_point,
                       promo_action, account_id, signin_aborted_mode,
                       is_sync_promo, turn_sync_on_signed_profile);
}

// static
Browser* SigninUiDelegate::EnsureBrowser(Profile* profile) {
  DCHECK(profile);
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  return displayer.browser();
}

}  // namespace signin_ui_util
