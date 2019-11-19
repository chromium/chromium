// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/process_dice_header_delegate_impl.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/common/url_constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

void RedirectToNtp(content::WebContents* contents) {
  VLOG(1) << "RedirectToNtp";
  contents->GetController().LoadURL(
      GURL(chrome::kChromeSearchLocalNtpUrl), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
}

}  // namespace

ProcessDiceHeaderDelegateImpl::ProcessDiceHeaderDelegateImpl(
    content::WebContents* web_contents,
    signin::AccountConsistencyMethod account_consistency,
    signin::IdentityManager* identity_manager,
    bool is_sync_signin_tab,
    EnableSyncCallback enable_sync_callback,
    ShowSigninErrorCallback show_signin_error_callback,
    const GURL& redirect_url)
    : content::WebContentsObserver(web_contents),
      account_consistency_(account_consistency),
      identity_manager_(identity_manager),
      enable_sync_callback_(std::move(enable_sync_callback)),
      show_signin_error_callback_(std::move(show_signin_error_callback)),
      is_sync_signin_tab_(is_sync_signin_tab),
      redirect_url_(redirect_url) {
  DCHECK(web_contents);
  DCHECK(identity_manager_);
  DCHECK(signin::DiceMethodGreaterOrEqual(
      account_consistency_, signin::AccountConsistencyMethod::kDiceMigration));
}

ProcessDiceHeaderDelegateImpl::~ProcessDiceHeaderDelegateImpl() = default;

bool ProcessDiceHeaderDelegateImpl::ShouldEnableSync() {
  if (identity_manager_->HasPrimaryAccount()) {
    VLOG(1) << "Do not start sync after web sign-in [already authenticated].";
    return false;
  }

  if (!is_sync_signin_tab_) {
    VLOG(1)
        << "Do not start sync after web sign-in [not a Chrome sign-in tab].";
    return false;
  }

  return true;
}

void ProcessDiceHeaderDelegateImpl::EnableSync(
    const CoreAccountId& account_id) {
  if (!ShouldEnableSync()) {
    // No special treatment is needed if the user is not enabling sync.
    return;
  }

  content::WebContents* web_contents = this->web_contents();
  VLOG(1) << "Start sync after web sign-in.";
  std::move(enable_sync_callback_).Run(web_contents, account_id);

  if (!web_contents)
    return;

  // After signing in to Chrome, the user should be redirected to the NTP,
  // unless specified otherwise.
  if (redirect_url_.is_empty()) {
    RedirectToNtp(web_contents);
    return;
  }

  DCHECK(redirect_url_.is_valid());
  web_contents->GetController().LoadURL(redirect_url_, content::Referrer(),
                                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
}

void ProcessDiceHeaderDelegateImpl::HandleTokenExchangeFailure(
    const std::string& email,
    const GoogleServiceAuthError& error) {
  DCHECK_NE(GoogleServiceAuthError::NONE, error.state());
  bool should_enable_sync = ShouldEnableSync();
  if (!should_enable_sync &&
      account_consistency_ != signin::AccountConsistencyMethod::kDice)
    return;

  content::WebContents* web_contents = this->web_contents();
  if (should_enable_sync && web_contents)
    RedirectToNtp(web_contents);

  // Show the error even if the WebContents was closed, because the user may be
  // signed out of the web.
  std::move(show_signin_error_callback_)
      .Run(web_contents, error.ToString(), email);
}
