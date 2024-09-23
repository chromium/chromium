// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/auth/chrome_login_performer.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/osauth/auth_factor_updater.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/early_prefs/early_prefs_reader.h"
#include "chromeos/ash/components/osauth/impl/early_login_auth_policy_connector.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

ChromeLoginPerformer::ChromeLoginPerformer(Delegate* delegate,
                                           AuthEventsRecorder* metrics_recorder)
    : LoginPerformer(delegate, metrics_recorder) {}

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
    if (delegate_) {
      delegate_->PolicyLoadFailed();
    } else {
      NOTREACHED_IN_MIGRATION();
    }
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
    if (delegate_) {
      delegate_->PolicyLoadFailed();
    } else {
      NOTREACHED_IN_MIGRATION();
    }
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
    const std::optional<user_manager::UserType>& user_type) {
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
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (connector->IsCloudManaged() && wildcard_match &&
      signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
          account_id.GetUserEmail())) {
    wildcard_login_checker_ = std::make_unique<policy::WildcardLoginChecker>();
    if (refresh_token.empty()) {
      NOTREACHED_IN_MIGRATION() << "Refresh token must be present.";
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

void ChromeLoginPerformer::LoadAndApplyEarlyPrefs(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  if (!base::FeatureList::IsEnabled(ash::features::kEnableEarlyPrefs)) {
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
  }
  base::FilePath early_prefs_dir;
  bool success = base::PathService::Get(chrome::DIR_CHROMEOS_HOMEDIR_MOUNT,
                                        &early_prefs_dir);
  CHECK(success);
  early_prefs_dir = early_prefs_dir.Append(context->GetUserIDHash());

  // Use TaskPriority::HIGHEST as this operation blocks
  // user login flow.
  early_prefs_reader_ = std::make_unique<EarlyPrefsReader>(
      early_prefs_dir, base::ThreadPool::CreateSequencedTaskRunner(
                           {base::MayBlock(), base::TaskPriority::HIGHEST,
                            base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  early_prefs_reader_->ReadFile(base::BindOnce(
      &ChromeLoginPerformer::OnEarlyPrefsRead, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback)));
}

void ChromeLoginPerformer::OnEarlyPrefsRead(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    bool success) {
  if (!success) {
    LOG(WARNING) << "No early prefs detected";
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
  }
  AuthEventsRecorder::Get()->OnEarlyPrefsParsed();
  AuthParts::Get()->RegisterEarlyLoginAuthPolicyConnector(
      std::make_unique<EarlyLoginAuthPolicyConnector>(
          context->GetAccountId(), std::move(early_prefs_reader_)));
  auth_factor_updater_ = std::make_unique<AuthFactorUpdater>(
      AuthParts::Get()->GetAuthPolicyConnector(), UserDataAuthClient::Get(),
      g_browser_process->local_state());
  auth_factor_updater_->Run(std::move(context), std::move(callback));
}

scoped_refptr<Authenticator> ChromeLoginPerformer::CreateAuthenticator() {
  return UserSessionManager::GetInstance()->CreateAuthenticator(this);
}

bool ChromeLoginPerformer::CheckPolicyForUser(const AccountId& account_id) {
  // Login is not allowed if policy could not be loaded for the account.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceLocalAccountPolicyService* policy_service =
      connector->GetDeviceLocalAccountPolicyService();
  return policy_service &&
         policy_service->IsPolicyAvailableForUser(account_id.GetUserEmail());
}
////////////////////////////////////////////////////////////////////////////////
// ChromeLoginPerformer, private:

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

}  // namespace ash
