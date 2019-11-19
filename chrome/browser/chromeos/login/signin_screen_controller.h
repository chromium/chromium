// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_SCREEN_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_SCREEN_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/screens/gaia_screen.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "components/user_manager/remove_user_delegate.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

class AccountId;

namespace chromeos {

class LoginDisplayWebUIHandler;
class OobeUI;

// Class that manages control flow between wizard screens. Wizard controller
// interacts with screen controllers to move the user between screens.
//
// This class is allocated when the signin or lock screen is actually visible to
// the user. It is a 'per-session' class; SignInScreenHandler, in comparsion, is
// tied to the WebContents lifetime and therefore may live beyond this class.
class SignInScreenController : public user_manager::RemoveUserDelegate,
                               public user_manager::UserManager::Observer {
 public:
  explicit SignInScreenController(OobeUI* oobe_ui);
  ~SignInScreenController() override;

  // Returns the default wizard controller if it has been created.
  static SignInScreenController* Get() { return instance_; }

  void SetWebUIHandler(LoginDisplayWebUIHandler* webui_handler);

  // Set up the list of users for user selection screen.
  // TODO(antrim): replace with querying for this data.
  void Init(const user_manager::UserList& users);

  // Called when signin screen is ready.
  void OnSigninScreenReady();

  // Query to send list of users to user selection screen.
  void SendUserList();

  // Runs OAauth token validity check.
  void CheckUserStatus(const AccountId& account_id);

  // Query to remove user with specified id.
  // TODO(antrim): move to user selection screen handler.
  void RemoveUser(const AccountId& account_id);

 private:
  // user_manager::RemoveUserDelegate implementation:
  void OnBeforeUserRemoved(const AccountId& account_id) override;
  void OnUserRemoved(const AccountId& account_id) override;

  // user_manager::UserManager::Observer implementation:
  void OnUserImageChanged(const user_manager::User& user) override;

  static SignInScreenController* instance_;

  OobeUI* oobe_ui_ = nullptr;

  // Reference to the WebUI handling layer for the login screen
  LoginDisplayWebUIHandler* webui_handler_ = nullptr;

  std::unique_ptr<GaiaScreen> gaia_screen_;
  std::unique_ptr<UserSelectionScreen> user_selection_screen_;

  base::WeakPtr<UserBoardView> user_board_view_;

  DISALLOW_COPY_AND_ASSIGN(SignInScreenController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_SCREEN_CONTROLLER_H_
