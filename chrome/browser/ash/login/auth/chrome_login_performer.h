// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_
#define CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/osauth/auth_factor_updater.h"
#include "chrome/browser/ash/policy/login/wildcard_login_checker.h"
#include "chromeos/ash/components/early_prefs/early_prefs_reader.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/authenticator.h"
#include "chromeos/ash/components/login/auth/login_performer.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/google_service_auth_error.h"

class AccountId;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class BrowserPolicyConnectorAsh;
class WildcardLoginChecker;
}  // namespace policy

namespace ash {

// This class implements chrome-specific elements of Login Performer.

class ChromeLoginPerformer : public LoginPerformer {
 public:
  // `local_state` and `browser_policy_connector_ash` must be non-null and
  // must outlive `this`.
  // `shared_url_loader_factory` must be non-null.
  ChromeLoginPerformer(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash,
      Delegate* delegate,
      AuthEventsRecorder* metrics_recorder);

  ChromeLoginPerformer(const ChromeLoginPerformer&) = delete;
  ChromeLoginPerformer& operator=(const ChromeLoginPerformer&) = delete;

  ~ChromeLoginPerformer() override;

  // LoginPerformer:
  bool IsUserAllowlisted(
      const AccountId& account_id,
      bool* wildcard_match,
      const std::optional<user_manager::UserType>& user_type) override;

  void LoadAndApplyEarlyPrefs(std::unique_ptr<UserContext> context,
                              AuthOperationCallback callback) override;

 protected:
  bool RunTrustedCheck(base::OnceClosure callback) override;
  // Runs `callback` unconditionally, but DidRunTrustedCheck() will only be run
  // itself sometimes, so ownership of `callback` should not be held in the
  // Callback pointing to DidRunTrustedCheck.
  void DidRunTrustedCheck(base::OnceClosure* callback);

  void RunOnlineAllowlistCheck(const AccountId& account_id,
                               bool wildcard_match,
                               const std::string& refresh_token,
                               base::OnceClosure success_callback,
                               base::OnceClosure failure_callback) override;

  scoped_refptr<Authenticator> CreateAuthenticator() override;
  bool CheckPolicyForUser(const AccountId& account_id) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSigninURLLoaderFactory()
      override;

 private:
  void OnlineWildcardLoginCheckCompleted(
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      policy::WildcardLoginChecker::Result result);
  void OnEarlyPrefsRead(std::unique_ptr<UserContext> context,
                        AuthOperationCallback callback,
                        bool success);

  const raw_ref<PrefService> local_state_;
  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;
  const raw_ref<policy::BrowserPolicyConnectorAsh>
      browser_policy_connector_ash_;

  std::unique_ptr<AuthFactorUpdater> auth_factor_updater_;
  std::unique_ptr<EarlyPrefsReader> early_prefs_reader_;
  // Used to verify logins that matched wildcard on the login allowlist.
  std::unique_ptr<policy::WildcardLoginChecker> wildcard_login_checker_;
  base::WeakPtrFactory<ChromeLoginPerformer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_
