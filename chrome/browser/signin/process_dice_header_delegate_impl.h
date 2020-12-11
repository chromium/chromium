// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_DELEGATE_IMPL_H_

#include "chrome/browser/signin/dice_response_handler.h"

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

class Profile;

class ProcessDiceHeaderDelegateImpl : public ProcessDiceHeaderDelegate,
                                      public content::WebContentsObserver {
 public:
  // Callback starting Sync.
  using EnableSyncCallback =
      base::OnceCallback<void(Profile*,
                              signin_metrics::AccessPoint,
                              signin_metrics::PromoAction,
                              signin_metrics::Reason,
                              content::WebContents*,
                              const CoreAccountId&)>;

  // Callback showing a signin error UI.
  using ShowSigninErrorCallback =
      base::OnceCallback<void(Profile*,
                              content::WebContents*,
                              const std::string& /* error_message */,
                              const std::string& /* email */)>;

  // |is_sync_signin_tab| is true if a sync signin flow has been started in that
  // tab.
  ProcessDiceHeaderDelegateImpl(
      content::WebContents* web_contents,
      EnableSyncCallback enable_sync_callback,
      ShowSigninErrorCallback show_signin_error_callback);
  ~ProcessDiceHeaderDelegateImpl() override;

  // ProcessDiceHeaderDelegate:
  void HandleTokenExchangeSuccess(CoreAccountId account_id,
                                  bool is_new_account) override;
  void EnableSync(const CoreAccountId& account_id) override;
  void HandleTokenExchangeFailure(const std::string& email,
                                  const GoogleServiceAuthError& error) override;

 private:
  // Returns true if sync should be enabled after the user signs in.
  bool ShouldEnableSync();

  Profile* profile_;
  EnableSyncCallback enable_sync_callback_;
  ShowSigninErrorCallback show_signin_error_callback_;
  bool is_sync_signin_tab_ = false;
  signin_metrics::AccessPoint access_point_ =
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN;
  signin_metrics::PromoAction promo_action_ =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  signin_metrics::Reason reason_ =
      signin_metrics::Reason::REASON_UNKNOWN_REASON;
  GURL redirect_url_;
  DISALLOW_COPY_AND_ASSIGN(ProcessDiceHeaderDelegateImpl);
};

#endif  // CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_DELEGATE_IMPL_H_
