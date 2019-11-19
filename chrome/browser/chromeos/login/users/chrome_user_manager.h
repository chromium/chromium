// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "chrome/browser/chromeos/login/users/affiliation.h"
#include "chrome/browser/chromeos/login/users/user_manager_interface.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_base.h"

namespace chromeos {

// Chrome specific interface of the UserManager.
class ChromeUserManager : public user_manager::UserManagerBase,
                          public UserManagerInterface {
 public:
  explicit ChromeUserManager(scoped_refptr<base::TaskRunner> task_runner);
  ~ChromeUserManager() override;

  // user_manager::UserManagerBase:
  bool IsCurrentUserNew() const override;
  void UpdateLoginState(const user_manager::User* active_user,
                        const user_manager::User* primary_user,
                        bool is_current_user_owner) const override;
  bool GetPlatformKnownUserId(const std::string& user_email,
                              const std::string& gaia_id,
                              AccountId* out_account_id) const override;

  // Returns current ChromeUserManager or NULL if instance hasn't been
  // yet initialized.
  static ChromeUserManager* Get();

  // Sets affiliation status for the user identified with |account_id|
  // judging by |user_affiliation_ids| and device affiliation IDs.
  virtual void SetUserAffiliation(
      const AccountId& account_id,
      const AffiliationIDSet& user_affiliation_ids) = 0;

  // Return whether the given user should be reported (see
  // policy::DeviceStatusCollector).
  virtual bool ShouldReportUser(const std::string& user_id) const = 0;

  // Checks whether 'DeviceLocalAccountManagedSessionEnabled' policy is enabled
  // for |active_user|.
  virtual bool IsManagedSessionEnabledForUser(
      const user_manager::User& active_user) const = 0;

  // Checks whether full management disclosure is needed for the public/managed
  // session login screen UI. Full disclosure is needed if the session is
  // managed and any risky extensions or network certificates are forced
  // through the policies.
  virtual bool IsFullManagementDisclosureNeeded(
      policy::DeviceLocalAccountPolicyBroker* broker) const = 0;

 private:
  LoginState::LoggedInUserType GetLoggedInUserType(
      const user_manager::User& active_user,
      bool is_current_user_owner) const;

  DISALLOW_COPY_AND_ASSIGN(ChromeUserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_H_
