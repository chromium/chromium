// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_NOTIFIER_ASH_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_NOTIFIER_ASH_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_error_controller.h"

class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin.

// Shows signin-related errors as notifications in Ash.
class SigninErrorNotifier : public SigninErrorController::Observer,
                            public KeyedService {
 public:
  SigninErrorNotifier(SigninErrorController* controller, Profile* profile);
  ~SigninErrorNotifier() override;

  // KeyedService:
  void Shutdown() override;

  // SigninErrorController::Observer:
  void OnErrorChanged() override;

 private:
  // Handles errors for the Device Account.
  // Displays a notification asking the user to Sign Out.
  void HandleDeviceAccountError();

  // Handles errors for Secondary Accounts.
  // Displays a notification that allows users to open crOS Account Manager UI.
  // |account_id| is the account identifier (used by the Token Service chain)
  // for the Secondary Account which received an error.
  void HandleSecondaryAccountError(const std::string& account_id);

  // |chromeos::AccountManager::GetAccounts| callback handler.
  void OnGetAccounts(
      const std::vector<chromeos::AccountManager::Account>& accounts);

  // Handles clicks on the Secondary Account reauth notification. See
  // |message_center::HandleNotificationClickDelegate|.
  void HandleSecondaryAccountReauthNotificationClick(
      base::Optional<int> button_index);

  base::string16 GetMessageBody(bool is_secondary_account_error) const;

  // The error controller to query for error details.
  SigninErrorController* error_controller_;

  // The Profile this service belongs to.
  Profile* const profile_;

  // A non-owning pointer to IdentityManager.
  signin::IdentityManager* const identity_manager_;

  // A non-owning pointer.
  chromeos::AccountManager* const account_manager_;

  // Used to keep track of the message center notifications.
  std::string device_account_notification_id_;
  std::string secondary_account_notification_id_;

  base::WeakPtrFactory<SigninErrorNotifier> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SigninErrorNotifier);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_NOTIFIER_ASH_H_
