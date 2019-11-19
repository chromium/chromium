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
#include "components/signin/public/base/account_consistency_method.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace signin {
class IdentityManager;
}

class ProcessDiceHeaderDelegateImpl : public ProcessDiceHeaderDelegate,
                                      public content::WebContentsObserver {
 public:
  // Callback starting Sync.
  using EnableSyncCallback =
      base::OnceCallback<void(content::WebContents*,
                              const CoreAccountId& /* account_id */)>;

  // Callback showing a signin error UI.
  using ShowSigninErrorCallback =
      base::OnceCallback<void(content::WebContents*,
                              const std::string& /* error_message */,
                              const std::string& /* email */)>;

  // |is_sync_signin_tab| is true if a sync signin flow has been started in that
  // tab.
  ProcessDiceHeaderDelegateImpl(
      content::WebContents* web_contents,
      signin::AccountConsistencyMethod account_consistency,
      signin::IdentityManager* identity_manager,
      bool is_sync_signin_tab,
      EnableSyncCallback enable_sync_callback,
      ShowSigninErrorCallback show_signin_error_callback,
      const GURL& redirect_url = GURL::EmptyGURL());
  ~ProcessDiceHeaderDelegateImpl() override;

  // ProcessDiceHeaderDelegate:
  void EnableSync(const CoreAccountId& account_id) override;
  void HandleTokenExchangeFailure(const std::string& email,
                                  const GoogleServiceAuthError& error) override;

 private:
  // Returns true if sync should be enabled after the user signs in.
  bool ShouldEnableSync();

  signin::AccountConsistencyMethod account_consistency_;
  signin::IdentityManager* identity_manager_;
  EnableSyncCallback enable_sync_callback_;
  ShowSigninErrorCallback show_signin_error_callback_;
  bool is_sync_signin_tab_;
  GURL redirect_url_;
  DISALLOW_COPY_AND_ASSIGN(ProcessDiceHeaderDelegateImpl);
};

#endif  // CHROME_BROWSER_SIGNIN_PROCESS_DICE_HEADER_DELEGATE_IMPL_H_
