// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_DELEGATE_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/dice_response_handler.h"
#include "components/signin/public/base/signin_metrics.h"

namespace content {
class WebContents;
}

struct CoreAccountInfo;
class Profile;
class SigninUIError;

class ProcessDiceHeaderDelegateImpl : public ProcessDiceHeaderDelegate {
 public:
  // Callback starting Sync.
  // This is similar to `DiceTabHelper::EnableSyncCallback` but is a once
  // callback (vs repeating).
  using EnableSyncCallback =
      base::OnceCallback<void(Profile*,
                              signin_metrics::AccessPoint,
                              signin_metrics::PromoAction,
                              content::WebContents*,
                              const CoreAccountInfo&)>;

  // Callback showing a signin error UI.
  // This is similar to `DiceTabHelper::ShowSigninErrorCallback` but is a once
  // callback (vs repeating).
  using ShowSigninErrorCallback = base::OnceCallback<
      void(Profile*, content::WebContents*, const SigninUIError&)>;

  // Callback in response to the receiving the signin header.
  // This is similar to `DiceTabHelper::OnSigninHeaderReceived` but is a once
  // callback (vs repeating).
  using OnSigninHeaderReceived = base::OnceCallback<void()>;

  // Helper function for creating `ProcessDiceHeaderDelegateImpl` from a
  // `content::WebContents`.
  static std::unique_ptr<ProcessDiceHeaderDelegateImpl> Create(
      content::WebContents* web_contents);

  // |is_sync_signin_tab| is true if a sync signin flow has been started in that
  // tab.
  ProcessDiceHeaderDelegateImpl(
      content::WebContents* web_contents,
      bool is_sync_signin_tab,
      signin_metrics::AccessPoint access_point,
      signin_metrics::PromoAction promo_action,
      GURL redirect_url,
      EnableSyncCallback enable_sync_callback,
      OnSigninHeaderReceived on_signin_header_received,
      ShowSigninErrorCallback show_signin_error_callback);

  ProcessDiceHeaderDelegateImpl(const ProcessDiceHeaderDelegateImpl&) = delete;
  ProcessDiceHeaderDelegateImpl& operator=(
      const ProcessDiceHeaderDelegateImpl&) = delete;

  ~ProcessDiceHeaderDelegateImpl() override;

  // ProcessDiceHeaderDelegate:
  void HandleTokenExchangeSuccess(CoreAccountId account_id,
                                  bool is_new_account) override;
  void EnableSync(const CoreAccountInfo& account_info) override;
  void HandleTokenExchangeFailure(const std::string& email,
                                  const GoogleServiceAuthError& error) override;
  signin_metrics::AccessPoint GetAccessPoint() override;
  void OnDiceSigninHeaderReceived() override;

 private:
  // Returns true if sync should be enabled after the user signs in.
  bool ShouldEnableSync();

  // Navigates to `redirect_url_`. Does nothing if the url is empty.
  void Redirect();

  const base::WeakPtr<content::WebContents> web_contents_;
  const raw_ref<Profile> profile_;
  const bool is_sync_signin_tab_;
  const signin_metrics::AccessPoint access_point_;
  const signin_metrics::PromoAction promo_action_;
  const GURL redirect_url_;
  EnableSyncCallback enable_sync_callback_;
  OnSigninHeaderReceived on_signin_header_received_;
  ShowSigninErrorCallback show_signin_error_callback_;
};

#endif  // CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_DELEGATE_IMPL_H_
