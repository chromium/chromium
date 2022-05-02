// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"

namespace signin_ui_util {

void SigninUiDelegate::ShowTurnSyncOnUI(
    Browser* browser,
    Profile* profile,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    signin_metrics::Reason signin_reason,
    const CoreAccountId& account_id,
    TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode) {
  // TurnSyncOnHelper is suicidal (it will delete itself once it finishes
  // enabling sync).
  new TurnSyncOnHelper(profile, EnsureBrowser(browser, profile), access_point,
                       promo_action, signin_reason, account_id,
                       signin_aborted_mode);
}

// static
Browser* SigninUiDelegate::EnsureBrowser(Browser* browser, Profile* profile) {
  DCHECK(!browser || browser->profile() == profile);
  DCHECK(profile);

  if (browser)
    return browser;

  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  return displayer.browser();
}

}  // namespace signin_ui_util
