// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth/chrome_login_performer.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_user_login_flow.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_constants.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_login_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

ChromeLoginPerformer::ChromeLoginPerformer(Delegate* delegate)
    : LoginPerformer(base::ThreadTaskRunnerHandle::Get(), delegate) {}

ChromeLoginPerformer::~ChromeLoginPerformer() {}

////////////////////////////////////////////////////////////////////////////////
// ChromeLoginPerformer, public:

bool ChromeLoginPerformer::RunTrustedCheck(base::OnceClosure callback) {
  CrosSettings* cros_settings = CrosSettings::Get();

  CrosSettingsProvider::TrustedStatus status =
      cros_settings->PrepareTrustedValues(
          base::BindOnce(&ChromeLoginPerformer::DidRunTrustedCheck,
                         weak_factory_.GetWeakPtr(), &callback));
  // Must not proceed without signature verification.
  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    if (delegate_)
      delegate_->PolicyLoadFailed();
    else
      NOTREACHED();
    return true;  // Some callback was called.
  } else if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    // Value of AllowNewUser setting is still not verified.
    // Another attempt will be invoked after verification completion.
    return false;
  } else {
    DCHECK(status == CrosSettingsProvider::TRUSTED);
    // CrosSettingsProvider::TRUSTED
    std::move(callback).Run();
    return true;  // Some callback was called.
  }
}

void ChromeLoginPerformer::DidRunTrustedCheck(base::OnceClosure* callback) {
  CrosSettings* cros_settings = CrosSettings::Get();

  CrosSettingsProvider::TrustedStatus status =
      cros_settings->PrepareTrustedValues(
          base::BindOnce(&ChromeLoginPerformer::DidRunTrustedCheck,
                         weak_factory_.GetWeakPtr(), callback));
  // Must not proceed without signature verification.
  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    if (delegate_)
      delegate_->PolicyLoadFailed();
    else
      NOTREACHED();
  } else if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    // Value of AllowNewUser setting is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  } else {
    DCHECK(status == CrosSettingsProvider::TRUSTED);
    std::move(*callback).Run();
  }
}

bool ChromeLoginPerformer::IsUserAllowlisted(
    const AccountId& account_id,
    bool* wildcard_match,
    const base::Optional<user_manager::UserType>& user_type) {
  return CrosSettings::Get()->IsUserAllowlisted(account_id.GetUserEmail(),
                                                wildcard_match, user_type);
}

void ChromeLoginPerformer::RunOnlineAllowlistCheck(
    const AccountId& account_id,
    bool wildcard_match,
    const std::string& refresh_token,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback) {
  // On cloud managed devices, reconfirm login permission with the server.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsCloudManaged() && wildcard_match &&
      !connector->IsNonEnterpriseUser(account_id.GetUserEmail())) {
    wildcard_login_checker_.reset(new policy::WildcardLoginChecker());
    if (refresh_token.empty()) {
      NOTREACHED() << "Refresh token must be present.";
      OnlineWildcardLoginCheckCompleted(
          std::move(success_callback), std::move(failure_callback),
          policy::WildcardLoginChecker::RESULT_FAILED);
    } else {
      wildcard_login_checker_->StartWithRefreshToken(
          refresh_token,
          base::BindOnce(
              &ChromeLoginPerformer::OnlineWildcardLoginCheckCompleted,
              weak_factory_.GetWeakPtr(), std::move(success_callback),
              std::move(failure_callback)));
    }
  } else {
    std::move(success_callback).Run();
  }
}

scoped_refptr<Authenticator> ChromeLoginPerformer::CreateAuthenticator() {
  return UserSessionManager::GetInstance()->CreateAuthenticator(this);
}

bool ChromeLoginPerformer::AreSupervisedUsersAllowed() {
  return user_manager::UserManager::Get()->AreSupervisedUsersAllowed();
}

bool ChromeLoginPerformer::UseExtendedAuthenticatorForSupervisedUser(
    const UserContext& user_context) {
  SupervisedUserAuthentication* authentication =
      ChromeUserManager::Get()->GetSupervisedUserManager()->GetAuthentication();
  return authentication->GetPasswordSchema(
             user_context.GetAccountId().GetUserEmail()) ==
         SupervisedUserAuthentication::SCHEMA_SALT_HASHED;
}

UserContext ChromeLoginPerformer::TransformSupervisedKey(
    const UserContext& context) {
  SupervisedUserAuthentication* authentication =
      ChromeUserManager::Get()->GetSupervisedUserManager()->GetAuthentication();
  return authentication->TransformKey(context);
}

void ChromeLoginPerformer::SetupSupervisedUserFlow(
    const AccountId& account_id) {
  SupervisedUserLoginFlow* new_flow = new SupervisedUserLoginFlow(account_id);
  new_flow->SetHost(ChromeUserManager::Get()->GetUserFlow(account_id)->host());
  ChromeUserManager::Get()->SetUserFlow(account_id, new_flow);
}

void ChromeLoginPerformer::SetupEasyUnlockUserFlow(
    const AccountId& account_id) {
  ChromeUserManager::Get()->SetUserFlow(
      account_id, new EasyUnlockUserLoginFlow(account_id));
}

bool ChromeLoginPerformer::CheckPolicyForUser(const AccountId& account_id) {
  // Login is not allowed if policy could not be loaded for the account.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceLocalAccountPolicyService* policy_service =
      connector->GetDeviceLocalAccountPolicyService();
  return policy_service &&
         policy_service->IsPolicyAvailableForUser(account_id.GetUserEmail());
}
////////////////////////////////////////////////////////////////////////////////
// ChromeLoginPerformer, private:

content::BrowserContext* ChromeLoginPerformer::GetSigninContext() {
  return ProfileHelper::GetSigninProfile();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeLoginPerformer::GetSigninURLLoaderFactory() {
  return login::GetSigninURLLoaderFactory();
}

void ChromeLoginPerformer::OnlineWildcardLoginCheckCompleted(
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    policy::WildcardLoginChecker::Result result) {
  if (result == policy::WildcardLoginChecker::RESULT_ALLOWED) {
    std::move(success_callback).Run();
  } else {
    std::move(failure_callback).Run();
  }
}

}  // namespace chromeos
