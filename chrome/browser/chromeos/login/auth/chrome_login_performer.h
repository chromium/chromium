// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/policy/wildcard_login_checker.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/authenticator.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "chromeos/login/auth/login_performer.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/google_service_auth_error.h"

class AccountId;

namespace policy {
class WildcardLoginChecker;
}

namespace chromeos {

// This class implements chrome-specific elements of Login Performer.

class ChromeLoginPerformer : public LoginPerformer {
 public:
  explicit ChromeLoginPerformer(Delegate* delegate);
  ~ChromeLoginPerformer() override;

  // LoginPerformer:
  bool IsUserAllowlisted(
      const AccountId& account_id,
      bool* wildcard_match,
      const base::Optional<user_manager::UserType>& user_type) override;

 protected:
  bool RunTrustedCheck(base::OnceClosure callback) override;
  // Runs |callback| unconditionally, but DidRunTrustedCheck() will only be run
  // itself sometimes, so ownership of |callback| should not be held in the
  // Callback pointing to DidRunTrustedCheck.
  void DidRunTrustedCheck(base::OnceClosure* callback);

  void RunOnlineAllowlistCheck(const AccountId& account_id,
                               bool wildcard_match,
                               const std::string& refresh_token,
                               base::OnceClosure success_callback,
                               base::OnceClosure failure_callback) override;
  bool AreSupervisedUsersAllowed() override;

  bool UseExtendedAuthenticatorForSupervisedUser(
      const UserContext& user_context) override;

  UserContext TransformSupervisedKey(const UserContext& context) override;

  void SetupSupervisedUserFlow(const AccountId& account_id) override;

  void SetupEasyUnlockUserFlow(const AccountId& account_id) override;

  scoped_refptr<Authenticator> CreateAuthenticator() override;
  bool CheckPolicyForUser(const AccountId& account_id) override;
  content::BrowserContext* GetSigninContext() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSigninURLLoaderFactory()
      override;

 private:
  void OnlineWildcardLoginCheckCompleted(
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      policy::WildcardLoginChecker::Result result);

  // Used to verify logins that matched wildcard on the login allowlist.
  std::unique_ptr<policy::WildcardLoginChecker> wildcard_login_checker_;
  base::WeakPtrFactory<ChromeLoginPerformer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeLoginPerformer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_
