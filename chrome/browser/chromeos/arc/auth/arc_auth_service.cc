// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/auth/arc_auth_service.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/account_manager/account_manager_migrator.h"
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/auth/arc_background_auth_code_fetcher.h"
#include "chrome/browser/chromeos/arc/auth/arc_robot_auth_code_fetcher.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/app_list/arc/arc_data_removal_dialog.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/arc_supervision_transition.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace arc {

namespace {

// Singleton factory for ArcAuthService.
class ArcAuthServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAuthService,
          ArcAuthServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAuthServiceFactory";

  static ArcAuthServiceFactory* GetInstance() {
    return base::Singleton<ArcAuthServiceFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcAuthServiceFactory>;

  ArcAuthServiceFactory() { DependsOn(IdentityManagerFactory::GetInstance()); }
  ~ArcAuthServiceFactory() override = default;
};

// Converts mojom::ArcSignInStatus into ProvisiningResult.
ProvisioningResult ConvertArcSignInResultToProvisioningResult(
    mojom::ArcSignInResult* result) {
  if (result->is_success()) {
    if (result->get_success() == mojom::ArcSignInSuccess::SUCCESS)
      return ProvisioningResult::SUCCESS;
    else
      return ProvisioningResult::SUCCESS_ALREADY_PROVISIONED;
  } else if (result->get_error()->is_cloud_provision_flow_error()) {
    return ProvisioningResult::CLOUD_PROVISION_FLOW_ERROR;
  } else if (result->get_error()->is_general_error()) {
#define MAP_GENERAL_ERROR(name)         \
  case mojom::GeneralSignInError::name: \
    return ProvisioningResult::name

    switch (result->get_error()->get_general_error()) {
      MAP_GENERAL_ERROR(UNKNOWN_ERROR);
      MAP_GENERAL_ERROR(MOJO_VERSION_MISMATCH);
      MAP_GENERAL_ERROR(PROVISIONING_TIMEOUT);
      MAP_GENERAL_ERROR(NO_NETWORK_CONNECTION);
      MAP_GENERAL_ERROR(CHROME_SERVER_COMMUNICATION_ERROR);
      MAP_GENERAL_ERROR(ARC_DISABLED);
      MAP_GENERAL_ERROR(UNSUPPORTED_ACCOUNT_TYPE);
      MAP_GENERAL_ERROR(CHROME_ACCOUNT_NOT_FOUND);
    }
#undef MAP_GENERAL_ERROR
  } else if (result->get_error()->is_checkin_error()) {
#define MAP_CHECKIN_ERROR(name)         \
  case mojom::DeviceCheckInError::name: \
    return ProvisioningResult::name

    switch (result->get_error()->get_checkin_error()) {
      MAP_CHECKIN_ERROR(DEVICE_CHECK_IN_FAILED);
      MAP_CHECKIN_ERROR(DEVICE_CHECK_IN_TIMEOUT);
      MAP_CHECKIN_ERROR(DEVICE_CHECK_IN_INTERNAL_ERROR);
    }
#undef MAP_CHECKIN_ERROR
  } else if (result->get_error()->is_gms_error()) {
#define MAP_GMS_ERROR(name)   \
  case mojom::GMSError::name: \
    return ProvisioningResult::name

    switch (result->get_error()->get_gms_error()) {
      MAP_GMS_ERROR(GMS_NETWORK_ERROR);
      MAP_GMS_ERROR(GMS_SERVICE_UNAVAILABLE);
      MAP_GMS_ERROR(GMS_BAD_AUTHENTICATION);
      MAP_GMS_ERROR(GMS_SIGN_IN_FAILED);
      MAP_GMS_ERROR(GMS_SIGN_IN_TIMEOUT);
      MAP_GMS_ERROR(GMS_SIGN_IN_INTERNAL_ERROR);
    }
#undef MAP_GMS_ERROR
  }

  NOTREACHED() << "unknown sign result";
  return ProvisioningResult::UNKNOWN_ERROR;
}

mojom::ChromeAccountType GetAccountType(const Profile* profile) {
  if (profile->IsChild())
    return mojom::ChromeAccountType::CHILD_ACCOUNT;

  if (IsActiveDirectoryUserForProfile(profile))
    return mojom::ChromeAccountType::ACTIVE_DIRECTORY_ACCOUNT;

  chromeos::DemoSession* demo_session = chromeos::DemoSession::Get();
  if (demo_session && demo_session->started()) {
    // Internally, demo mode is implemented as a public session, and should
    // generally follow normal robot account provisioning flow. Offline enrolled
    // demo mode is an exception, as it is expected to work purely offline, with
    // a (fake) robot account not known to auth service - this means that it has
    // to go through different, offline provisioning flow.
    DCHECK(IsRobotOrOfflineDemoAccountMode());
    return demo_session->offline_enrolled()
               ? mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT
               : mojom::ChromeAccountType::ROBOT_ACCOUNT;
  }

  return IsRobotOrOfflineDemoAccountMode()
             ? mojom::ChromeAccountType::ROBOT_ACCOUNT
             : mojom::ChromeAccountType::USER_ACCOUNT;
}

mojom::AccountInfoPtr CreateAccountInfo(bool is_enforced,
                                        const std::string& auth_info,
                                        const std::string& account_name,
                                        mojom::ChromeAccountType account_type,
                                        bool is_managed) {
  mojom::AccountInfoPtr account_info = mojom::AccountInfo::New();
  account_info->account_name = account_name;
  if (account_type == mojom::ChromeAccountType::ACTIVE_DIRECTORY_ACCOUNT) {
    account_info->enrollment_token = auth_info;
  } else {
    if (!is_enforced)
      account_info->auth_code = base::nullopt;
    else
      account_info->auth_code = auth_info;
  }
  account_info->account_type = account_type;
  account_info->is_managed = is_managed;
  return account_info;
}

bool IsPrimaryGaiaAccount(const std::string& gaia_id) {
  // |GetPrimaryUser| is fine because ARC is only available on the first
  // (Primary) account that participates in multi-signin.
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(user);
  return user->GetAccountId().GetAccountType() == AccountType::GOOGLE &&
         user->GetAccountId().GetGaiaId() == gaia_id;
}

bool IsPrimaryOrDeviceLocalAccount(
    const signin::IdentityManager* identity_manager,
    const std::string& account_name) {
  // |GetPrimaryUser| is fine because ARC is only available on the first
  // (Primary) account that participates in multi-signin.
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(user);

  // There is no Gaia user for device local accounts, but in this case there is
  // always only a primary account.
  if (user->IsDeviceLocalAccount())
    return true;

  const base::Optional<AccountInfo> account_info =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              account_name);
  if (!account_info)
    return false;

  const std::string& gaia_id = account_info->gaia;
  DCHECK(!gaia_id.empty());
  return IsPrimaryGaiaAccount(gaia_id);
}

void TriggerAccountManagerMigrationsIfRequired(Profile* profile) {
  if (!chromeos::IsAccountManagerAvailable(profile))
    return;

  chromeos::AccountManagerMigrator* const migrator =
      chromeos::AccountManagerMigratorFactory::GetForBrowserContext(profile);
  if (!migrator) {
    // Migrator can be null for ephemeral and kiosk sessions. Ignore those cases
    // since there are no accounts to be migrated in that case.
    return;
  }
  const base::Optional<chromeos::AccountMigrationRunner::MigrationResult>
      last_migration_run_result = migrator->GetLastMigrationRunResult();

  if (!last_migration_run_result)
    return;

  if (last_migration_run_result->final_status !=
      chromeos::AccountMigrationRunner::Status::kFailure) {
    return;
  }

  if (last_migration_run_result->failed_step_id !=
      chromeos::AccountManagerMigrator::kArcAccountsMigrationId) {
    // Migrations failed but not because of ARC. ARC should not try to re-run
    // migrations in this case.
    return;
  }

  // Migrations are idempotent and safe to run multiple times. It may have
  // happened that ARC migrations timed out at the start of the session. Give
  // it a chance to run again.
  migrator->Start();
}

// See //components/arc/mojom/auth.mojom RequestPrimaryAccount() for the spec.
// See also go/arc-primary-account.
std::string GetAccountName(Profile* profile) {
  switch (GetAccountType(profile)) {
    case mojom::ChromeAccountType::USER_ACCOUNT:
    case mojom::ChromeAccountType::CHILD_ACCOUNT:
      // IdentityManager::GetPrimaryAccountInfo(
      //    signin::ConsentLevel::kNotRequired).email might be more appropriate
      // here, but this is what we have done historically.
      return chromeos::ProfileHelper::Get()
          ->GetUserByProfile(profile)
          ->GetDisplayEmail();
    case mojom::ChromeAccountType::ROBOT_ACCOUNT:
    case mojom::ChromeAccountType::ACTIVE_DIRECTORY_ACCOUNT:
    case mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT:
      return std::string();
    case mojom::ChromeAccountType::UNKNOWN:
      NOTREACHED();
      return std::string();
  }
}

void OnFetchPrimaryAccountInfoCompleted(
    ArcAuthService::RequestAccountInfoCallback callback,
    bool persistent_error,
    mojom::ArcSignInStatus status,
    mojom::AccountInfoPtr account_info) {
  std::move(callback).Run(std::move(status), std::move(account_info),
                          persistent_error);
}

}  // namespace

// static
const char ArcAuthService::kArcServiceName[] = "arc::ArcAuthService";

// static
ArcAuthService* ArcAuthService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAuthServiceFactory::GetForBrowserContext(context);
}

ArcAuthService::ArcAuthService(content::BrowserContext* browser_context,
                               ArcBridgeService* arc_bridge_service)
    : profile_(Profile::FromBrowserContext(browser_context)),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)),
      arc_bridge_service_(arc_bridge_service),
      url_loader_factory_(
          content::BrowserContext::GetDefaultStoragePartition(profile_)
              ->GetURLLoaderFactoryForBrowserProcess()) {
  arc_bridge_service_->auth()->SetHost(this);
  arc_bridge_service_->auth()->AddObserver(this);

  ArcSessionManager::Get()->AddObserver(this);
  identity_manager_->AddObserver(this);
}

ArcAuthService::~ArcAuthService() {
  ArcSessionManager::Get()->RemoveObserver(this);
  arc_bridge_service_->auth()->RemoveObserver(this);
  arc_bridge_service_->auth()->SetHost(nullptr);
}

void ArcAuthService::GetGoogleAccountsInArc(
    GetGoogleAccountsInArcCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(pending_get_arc_accounts_callback_.is_null())
      << "Cannot have more than one pending GetGoogleAccountsInArc request";

  if (!arc::IsArcProvisioned(profile_)) {
    std::move(callback).Run(std::vector<mojom::ArcAccountInfoPtr>());
    return;
  }

  if (!arc_bridge_service_->auth()->IsConnected()) {
    pending_get_arc_accounts_callback_ = std::move(callback);
    // Will be retried in |OnConnectionReady|.
    return;
  }

  DispatchAccountsInArc(std::move(callback));
}

void ArcAuthService::RequestPrimaryAccount(
    RequestPrimaryAccountCallback callback) {
  std::move(callback).Run(GetAccountName(profile_), GetAccountType(profile_));
}

void ArcAuthService::OnConnectionReady() {
  // |TriggerAccountsPushToArc()| will not be triggered for the first session,
  // when ARC has not been provisioned yet. For the first session, an account
  // push will be triggered by |OnArcInitialStart()|, after a successful device
  // provisioning.
  // For the second and subsequent sessions,
  // |ArcSessionManager::Get()->IsArcProvisioned()| will be |true|.
  if (arc::IsArcProvisioned(profile_)) {
    TriggerAccountManagerMigrationsIfRequired(profile_);
    TriggerAccountsPushToArc(false /* filter_primary_account */);
  }

  if (pending_get_arc_accounts_callback_)
    DispatchAccountsInArc(std::move(pending_get_arc_accounts_callback_));

  // Report main account resolution status for provisioned devices.
  if (!IsArcProvisioned(profile_))
    return;

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->auth(),
                                               GetMainAccountResolutionStatus);
  if (!instance)
    return;

  instance->GetMainAccountResolutionStatus(
      base::BindOnce(&ArcAuthService::OnMainAccountResolutionStatus,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcAuthService::OnConnectionClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pending_token_requests_.clear();
}

void ArcAuthService::OnAuthorizationResult(mojom::ArcSignInResultPtr result,
                                           mojom::ArcSignInAccountPtr account) {
  const ProvisioningResult provisioning_result =
      ConvertArcSignInResultToProvisioningResult(result.get());

  if (account->is_initial_signin()) {
    // UMA for initial signin is updated from ArcSessionManager.
    ArcSessionManager::Get()->OnProvisioningFinished(
        provisioning_result,
        result->is_error() ? std::move(result->get_error()) : nullptr);
    return;
  }

  // Re-auth shouldn't be triggered for non-Gaia device local accounts.
  if (!user_manager::UserManager::Get()->IsLoggedInAsUserWithGaiaAccount()) {
    NOTREACHED() << "Shouldn't re-auth for non-Gaia accounts";
    return;
  }

  if (!account->is_account_name() || !account->get_account_name() ||
      account->get_account_name().value().empty() ||
      IsPrimaryOrDeviceLocalAccount(identity_manager_,
                                    account->get_account_name().value())) {
    // Reauthorization for the Primary Account.
    // The check for |!account_name.has_value()| is for backwards compatibility
    // with older ARC versions, for which Mojo will set |account_name| to
    // empty/null.
    DCHECK_NE(ProvisioningResult::SUCCESS_ALREADY_PROVISIONED,
              provisioning_result);
    UpdateReauthorizationResultUMA(provisioning_result, profile_);
  } else {
    UpdateSecondarySigninResultUMA(provisioning_result);
  }
}

void ArcAuthService::ReportMetrics(mojom::MetricsType metrics_type,
                                   int32_t value) {
  switch (metrics_type) {
    case mojom::MetricsType::NETWORK_WAITING_TIME_MILLISECONDS:
      UpdateAuthTiming("Arc.Auth.NetworkWait.TimeDelta",
                       base::TimeDelta::FromMilliseconds(value), profile_);
      break;
    case mojom::MetricsType::CHECKIN_ATTEMPTS:
      UpdateAuthCheckinAttempts(value, profile_);
      break;
    case mojom::MetricsType::CHECKIN_TIME_MILLISECONDS:
      UpdateAuthTiming("Arc.Auth.Checkin.TimeDelta",
                       base::TimeDelta::FromMilliseconds(value), profile_);
      break;
    case mojom::MetricsType::SIGNIN_TIME_MILLISECONDS:
      UpdateAuthTiming("Arc.Auth.SignIn.TimeDelta",
                       base::TimeDelta::FromMilliseconds(value), profile_);
      break;
    case mojom::MetricsType::ACCOUNT_CHECK_MILLISECONDS:
      UpdateAuthTiming("Arc.Auth.AccountCheck.TimeDelta",
                       base::TimeDelta::FromMilliseconds(value), profile_);
      break;
  }
}

void ArcAuthService::ReportAccountCheckStatus(
    mojom::AccountCheckStatus status) {
  UpdateAuthAccountCheckStatus(status, profile_);
}

void ArcAuthService::ReportSupervisionChangeStatus(
    mojom::SupervisionChangeStatus status) {
  UpdateSupervisionTransitionResultUMA(status);
  switch (status) {
    case mojom::SupervisionChangeStatus::CLOUD_DPC_DISABLED:
    case mojom::SupervisionChangeStatus::CLOUD_DPC_ALREADY_DISABLED:
    case mojom::SupervisionChangeStatus::CLOUD_DPC_ENABLED:
    case mojom::SupervisionChangeStatus::CLOUD_DPC_ALREADY_ENABLED:
      profile_->GetPrefs()->SetInteger(
          prefs::kArcSupervisionTransition,
          static_cast<int>(ArcSupervisionTransition::NO_TRANSITION));
      // TODO(brunokim): notify potential observers.
      break;
    case mojom::SupervisionChangeStatus::CLOUD_DPC_DISABLING_FAILED:
    case mojom::SupervisionChangeStatus::CLOUD_DPC_ENABLING_FAILED:
      LOG(ERROR) << "Child transition failed: " << status;
      ShowDataRemovalConfirmationDialog(
          profile_, base::BindOnce(&ArcAuthService::OnDataRemovalAccepted,
                                   weak_ptr_factory_.GetWeakPtr()));
      break;
    case mojom::SupervisionChangeStatus::INVALID_SUPERVISION_STATE:
      NOTREACHED() << "Invalid status of child transition: " << status;
  }
}

void ArcAuthService::RequestPrimaryAccountInfo(
    RequestPrimaryAccountInfoCallback callback) {
  // This is the provisioning flow.
  FetchPrimaryAccountInfo(true /* initial_signin */, std::move(callback));
}

void ArcAuthService::RequestAccountInfo(const std::string& account_name,
                                        RequestAccountInfoCallback callback) {
  // This is the post provisioning flow.
  // This request could have come for re-authenticating an existing account in
  // ARC, or for signing in a new Secondary Account.

  // Check if |account_name| points to a Secondary Account.
  if (!IsPrimaryOrDeviceLocalAccount(identity_manager_, account_name)) {
    FetchSecondaryAccountInfo(account_name, std::move(callback));
    return;
  }

  // TODO(solovey): Check secondary account ARC sign-in statistics and send
  // |persistent_error| == true for primary account for cases when refresh token
  // has persistent error.
  FetchPrimaryAccountInfo(
      false /* initial_signin */,
      base::BindOnce(&OnFetchPrimaryAccountInfoCompleted, std::move(callback),
                     false /* persistent_error */));
}

void ArcAuthService::FetchPrimaryAccountInfo(
    bool initial_signin,
    RequestPrimaryAccountInfoCallback callback) {
  const mojom::ChromeAccountType account_type = GetAccountType(profile_);

  if (IsArcOptInVerificationDisabled()) {
    std::move(callback).Run(
        mojom::ArcSignInStatus::SUCCESS,
        CreateAccountInfo(false /* is_enforced */,
                          std::string() /* auth_info */,
                          std::string() /* auth_name */, account_type,
                          policy_util::IsAccountManaged(profile_)));
    return;
  }

  if (IsActiveDirectoryUserForProfile(profile_)) {
    // For Active Directory enrolled devices, we get an enrollment token for a
    // managed Google Play account from DMServer.
    auto enrollment_token_fetcher =
        std::make_unique<ArcActiveDirectoryEnrollmentTokenFetcher>(
            ArcSessionManager::Get()->support_host());

    // Add the request to |pending_token_requests_| first, before starting a
    // token fetch. In case the callback is called immediately, we do not want
    // to add an already completed request to |pending_token_requests_|.
    auto* enrollment_token_fetcher_ptr = enrollment_token_fetcher.get();
    pending_token_requests_.emplace_back(std::move(enrollment_token_fetcher));
    enrollment_token_fetcher_ptr->Fetch(
        base::BindOnce(&ArcAuthService::OnActiveDirectoryEnrollmentTokenFetched,
                       weak_ptr_factory_.GetWeakPtr(),
                       enrollment_token_fetcher_ptr, std::move(callback)));
    return;
  }

  if (account_type == mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT) {
    // Skip account auth code fetch for offline enrolled demo mode.
    std::move(callback).Run(
        mojom::ArcSignInStatus::SUCCESS,
        CreateAccountInfo(true /* is_enforced */, std::string() /* auth_info */,
                          std::string() /* auth_name */, account_type,
                          true /* is_managed */));
    return;
  }

  // For non-AD enrolled devices an auth code is fetched.
  std::unique_ptr<ArcAuthCodeFetcher> auth_code_fetcher;
  if (account_type == mojom::ChromeAccountType::ROBOT_ACCOUNT) {
    // For robot accounts, which are used in kiosk and public session mode
    // (which includes online demo sessions), use Robot auth code fetching.
    auth_code_fetcher = std::make_unique<ArcRobotAuthCodeFetcher>();
    if (url_loader_factory_for_testing_set_) {
      static_cast<ArcRobotAuthCodeFetcher*>(auth_code_fetcher.get())
          ->SetURLLoaderFactoryForTesting(url_loader_factory_);
    }
  } else {
    // Optionally retrieve auth code in silent mode. Use the "unconsented"
    // primary account because this class doesn't care about browser sync
    // consent.
    DCHECK(identity_manager_->HasPrimaryAccount(
        signin::ConsentLevel::kNotRequired));
    auth_code_fetcher = CreateArcBackgroundAuthCodeFetcher(
        identity_manager_->GetPrimaryAccountId(
            signin::ConsentLevel::kNotRequired),
        initial_signin);
  }

  // Add the request to |pending_token_requests_| first, before starting a token
  // fetch. In case the callback is called immediately, we do not want to add an
  // already completed request to |pending_token_requests_|.
  auto* auth_code_fetcher_ptr = auth_code_fetcher.get();
  pending_token_requests_.emplace_back(std::move(auth_code_fetcher));
  auth_code_fetcher_ptr->Fetch(
      base::BindOnce(&ArcAuthService::OnPrimaryAccountAuthCodeFetched,
                     weak_ptr_factory_.GetWeakPtr(), auth_code_fetcher_ptr,
                     std::move(callback)));
}

void ArcAuthService::IsAccountManagerAvailable(
    IsAccountManagerAvailableCallback callback) {
  std::move(callback).Run(chromeos::IsAccountManagerAvailable(profile_));
}

void ArcAuthService::HandleAddAccountRequest() {
  DCHECK(chromeos::IsAccountManagerAvailable(profile_));

  chromeos::InlineLoginDialogChromeOS::Show(
      chromeos::InlineLoginDialogChromeOS::Source::kArc);
}

void ArcAuthService::HandleRemoveAccountRequest(const std::string& email) {
  DCHECK(chromeos::IsAccountManagerAvailable(profile_));

  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kMyAccountsSubpagePath);
}

void ArcAuthService::HandleUpdateCredentialsRequest(const std::string& email) {
  DCHECK(chromeos::IsAccountManagerAvailable(profile_));

  chromeos::InlineLoginDialogChromeOS::Show(
      email, chromeos::InlineLoginDialogChromeOS::Source::kArc);
}

void ArcAuthService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // TODO(sinhak): Identity Manager is specific to a Profile. Move this to a
  // proper Profile independent entity once we have that.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!chromeos::IsAccountManagerAvailable(profile_))
    return;

  // Ignore the update if ARC has not been provisioned yet.
  if (!arc::IsArcProvisioned(profile_))
    return;

  // For child device accounts do not allow the propagation of secondary
  // accounts from Chrome OS Account Manager to ARC unless experimental feature
  // is enabled.
  if (!arc::IsSecondaryAccountForChildEnabled() && profile_->IsChild() &&
      !IsPrimaryGaiaAccount(account_info.gaia)) {
    return;
  }

  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id)) {
    VLOG(1) << "Ignoring account update due to lack of a valid token: "
            << account_info.email;
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->auth(),
                                               OnAccountUpdated);
  if (!instance)
    return;

  const std::string account_name = account_info.email;
  DCHECK(!account_name.empty());
  instance->OnAccountUpdated(account_name, mojom::AccountUpdateType::UPSERT);
}

void ArcAuthService::OnExtendedAccountInfoRemoved(
    const AccountInfo& account_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!chromeos::IsAccountManagerAvailable(profile_))
    return;

  DCHECK(!IsPrimaryGaiaAccount(account_info.gaia));

  // Ignore the update if ARC has not been provisioned yet.
  if (!arc::IsArcProvisioned(profile_))
    return;

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->auth(),
                                               OnAccountUpdated);
  if (!instance)
    return;

  DCHECK(!account_info.email.empty());
  instance->OnAccountUpdated(account_info.email,
                             mojom::AccountUpdateType::REMOVAL);
}

void ArcAuthService::OnArcInitialStart() {
  TriggerAccountsPushToArc(true /* filter_primary_account */);
}

void ArcAuthService::Shutdown() {
  identity_manager_->RemoveObserver(this);
}

void ArcAuthService::OnActiveDirectoryEnrollmentTokenFetched(
    ArcActiveDirectoryEnrollmentTokenFetcher* fetcher,
    RequestPrimaryAccountInfoCallback callback,
    ArcActiveDirectoryEnrollmentTokenFetcher::Status status,
    const std::string& enrollment_token,
    const std::string& user_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // |fetcher| will be invalid after this.
  DeletePendingTokenRequest(fetcher);

  switch (status) {
    case ArcActiveDirectoryEnrollmentTokenFetcher::Status::SUCCESS: {
      // Save user_id to the user profile.
      profile_->GetPrefs()->SetString(prefs::kArcActiveDirectoryPlayUserId,
                                      user_id);

      // Send enrollment token to ARC.
      std::move(callback).Run(
          mojom::ArcSignInStatus::SUCCESS,
          CreateAccountInfo(true /* is_enforced */, enrollment_token,
                            std::string() /* account_name */,
                            mojom::ChromeAccountType::ACTIVE_DIRECTORY_ACCOUNT,
                            true /* is_managed */));
      break;
    }
    case ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE: {
      // Send error to ARC.
      std::move(callback).Run(
          mojom::ArcSignInStatus::CHROME_SERVER_COMMUNICATION_ERROR, nullptr);
      break;
    }
    case ArcActiveDirectoryEnrollmentTokenFetcher::Status::ARC_DISABLED: {
      // Send error to ARC.
      std::move(callback).Run(mojom::ArcSignInStatus::ARC_DISABLED, nullptr);
      break;
    }
  }
}

void ArcAuthService::OnPrimaryAccountAuthCodeFetched(
    ArcAuthCodeFetcher* fetcher,
    RequestPrimaryAccountInfoCallback callback,
    bool success,
    const std::string& auth_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // |fetcher| will be invalid after this.
  DeletePendingTokenRequest(fetcher);

  if (success) {
    const std::string& full_account_id = GetAccountName(profile_);
    std::move(callback).Run(
        mojom::ArcSignInStatus::SUCCESS,
        CreateAccountInfo(!IsArcOptInVerificationDisabled(), auth_code,
                          full_account_id, GetAccountType(profile_),
                          policy_util::IsAccountManaged(profile_)));
  } else if (chromeos::DemoSession::Get() &&
             chromeos::DemoSession::Get()->started()) {
    // For demo sessions, if auth code fetch failed (e.g. because the device is
    // offline), fall back to accountless offline demo mode provisioning.
    std::move(callback).Run(
        mojom::ArcSignInStatus::SUCCESS,
        CreateAccountInfo(true /* is_enforced */, std::string() /* auth_info */,
                          std::string() /* auth_name */,
                          mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
                          true /* is_managed */));
  } else {
    // Send error to ARC.
    std::move(callback).Run(
        mojom::ArcSignInStatus::CHROME_SERVER_COMMUNICATION_ERROR, nullptr);
  }
}

void ArcAuthService::FetchSecondaryAccountInfo(
    const std::string& account_name,
    RequestAccountInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Optional<AccountInfo> account_info =
      identity_manager_
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              account_name);
  if (!account_info.has_value()) {
    // Account is in ARC, but not in Chrome OS Account Manager.
    std::move(callback).Run(mojom::ArcSignInStatus::CHROME_ACCOUNT_NOT_FOUND,
                            nullptr /* account_info */,
                            true /* persistent_error */);
    return;
  }

  const CoreAccountId& account_id = account_info->account_id;
  DCHECK(!account_id.empty());

  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id)) {
    std::move(callback).Run(
        mojom::ArcSignInStatus::CHROME_SERVER_COMMUNICATION_ERROR,
        nullptr /* account_info */, true /* persistent_error */);
    return;
  }

  std::unique_ptr<ArcBackgroundAuthCodeFetcher> fetcher =
      CreateArcBackgroundAuthCodeFetcher(account_id,
                                         false /* initial_signin */);

  // Add the request to |pending_token_requests_| first, before starting a
  // token fetch. In case the callback is called immediately, we do not want
  // to add an already completed request to |pending_token_requests_|.
  auto* fetcher_ptr = fetcher.get();
  pending_token_requests_.emplace_back(std::move(fetcher));
  fetcher_ptr->Fetch(
      base::BindOnce(&ArcAuthService::OnSecondaryAccountAuthCodeFetched,
                     weak_ptr_factory_.GetWeakPtr(), account_name, fetcher_ptr,
                     std::move(callback)));
}

void ArcAuthService::OnSecondaryAccountAuthCodeFetched(
    const std::string& account_name,
    ArcBackgroundAuthCodeFetcher* fetcher,
    RequestAccountInfoCallback callback,
    bool success,
    const std::string& auth_code) {
  // |fetcher| will be invalid after this.
  DeletePendingTokenRequest(fetcher);

  if (success) {
    std::move(callback).Run(
        mojom::ArcSignInStatus::SUCCESS,
        CreateAccountInfo(true /* is_enforced */, auth_code, account_name,
                          mojom::ChromeAccountType::USER_ACCOUNT,
                          false /* is_managed */),
        false /* persistent_error*/);
    return;
  }

  base::Optional<AccountInfo> account_info =
      identity_manager_
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              account_name);
  // Take care of the case when the user removes an account immediately after
  // adding/re-authenticating it.
  if (account_info.has_value()) {
    const bool is_persistent_error =
        identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info->account_id);
    std::move(callback).Run(
        mojom::ArcSignInStatus::CHROME_SERVER_COMMUNICATION_ERROR,
        nullptr /* account_info */, is_persistent_error);
    return;
  }

  std::move(callback).Run(mojom::ArcSignInStatus::CHROME_ACCOUNT_NOT_FOUND,
                          nullptr /* account_info */, true);
}

void ArcAuthService::DeletePendingTokenRequest(ArcFetcherBase* fetcher) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (auto it = pending_token_requests_.begin();
       it != pending_token_requests_.end(); ++it) {
    if (it->get() == fetcher) {
      pending_token_requests_.erase(it);
      return;
    }
  }

  // We should not have received a call to delete a |fetcher| that was not in
  // |pending_token_requests_|.
  NOTREACHED();
}

void ArcAuthService::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = std::move(url_loader_factory);
  url_loader_factory_for_testing_set_ = true;
}

void ArcAuthService::OnDataRemovalAccepted(bool accepted) {
  if (!accepted)
    return;
  if (!IsArcPlayStoreEnabledForProfile(profile_))
    return;
  VLOG(1)
      << "Request for data removal on child transition failure is confirmed";
  ArcSessionManager::Get()->RequestArcDataRemoval();
  ArcSessionManager::Get()->StopAndEnableArc();
}

std::unique_ptr<ArcBackgroundAuthCodeFetcher>
ArcAuthService::CreateArcBackgroundAuthCodeFetcher(
    const CoreAccountId& account_id,
    bool initial_signin) {
  base::Optional<AccountInfo> account_info =
      identity_manager_
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id);
  DCHECK(account_info.has_value());
  auto fetcher = std::make_unique<ArcBackgroundAuthCodeFetcher>(
      url_loader_factory_, profile_, account_id, initial_signin,
      IsPrimaryGaiaAccount(account_info.value().gaia));

  return fetcher;
}

void ArcAuthService::TriggerAccountsPushToArc(bool filter_primary_account) {
  if (!chromeos::IsAccountManagerAvailable(profile_))
    return;

  const std::vector<CoreAccountInfo> accounts =
      identity_manager_->GetAccountsWithRefreshTokens();
  for (const CoreAccountInfo& account : accounts) {
    if (filter_primary_account && IsPrimaryGaiaAccount(account.gaia))
      continue;

    OnRefreshTokenUpdatedForAccount(account);
  }
}

void ArcAuthService::DispatchAccountsInArc(
    GetGoogleAccountsInArcCallback callback) {
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->auth(),
                                               GetGoogleAccounts);
  if (!instance) {
    // Complete the callback so that it is not kept waiting forever.
    std::move(callback).Run(std::vector<mojom::ArcAccountInfoPtr>());
    return;
  }

  instance->GetGoogleAccounts(std::move(callback));
}

void ArcAuthService::OnMainAccountResolutionStatus(
    mojom::MainAccountResolutionStatus status) {
  UpdateMainAccountResolutionStatus(profile_, status);
}

}  // namespace arc
