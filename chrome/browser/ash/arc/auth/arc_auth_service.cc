// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/auth/arc_auth_service.h"

#include <optional>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/mojom/auth.mojom-shared.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_management_transition.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_switches.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/account_manager/account_apps_availability_factory.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/ash/app_list/arc/arc_data_removal_dialog.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/auth/arc_background_auth_code_fetcher.h"
#include "chrome/browser/ash/arc/auth/arc_robot_auth_code_fetcher.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

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

  ArcAuthServiceFactory() {
    DependsOn(IdentityManagerFactory::GetInstance());
    DependsOn(ash::AccountAppsAvailabilityFactory::GetInstance());
  }
  ~ArcAuthServiceFactory() override = default;
};

mojom::ChromeAccountType GetAccountType(const Profile* profile) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line);
  if (command_line->HasSwitch(
          ash::switches::kDemoModeForceArcOfflineProvision)) {
    return mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT;
  }

  if (profile->IsChild()) {
    return mojom::ChromeAccountType::CHILD_ACCOUNT;
  }

  auto* demo_session = ash::DemoSession::Get();
  if (demo_session && demo_session->started()) {
    // Internally, demo mode is implemented as a public session, and should
    // generally follow normal robot account provisioning flow. Offline enrolled
    // demo mode is an exception, as it is expected to work purely offline, with
    // a (fake) robot account not known to auth service - this means that it has
    // to go through different, offline provisioning flow.
    DCHECK(IsRobotOrOfflineDemoAccountMode());
    return mojom::ChromeAccountType::ROBOT_ACCOUNT;
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

  if (!is_enforced) {
    account_info->auth_code = std::nullopt;
  } else {
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
  if (user->IsDeviceLocalAccount()) {
    return true;
  }

  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByEmailAddress(account_name);
  if (account_info.IsEmpty()) {
    return false;
  }

  DCHECK(!account_info.gaia.empty());
  return IsPrimaryGaiaAccount(account_info.gaia);
}

// See //ash/components/arc/mojom/auth.mojom RequestPrimaryAccount() for the
// spec. See also go/arc-primary-account.
std::string GetAccountName(Profile* profile) {
  switch (GetAccountType(profile)) {
    case mojom::ChromeAccountType::USER_ACCOUNT:
      [[fallthrough]];
    case mojom::ChromeAccountType::CHILD_ACCOUNT:
      // IdentityManager::GetPrimaryAccountInfo(
      //    signin::ConsentLevel::kSignin).email might be more appropriate
      // here, but this is what we have done historically.
      return ash::ProfileHelper::Get()
          ->GetUserByProfile(profile)
          ->GetDisplayEmail();
    case mojom::ChromeAccountType::ROBOT_ACCOUNT:
      [[fallthrough]];
    case mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT:
      return std::string();
    case mojom::ChromeAccountType::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

void OnFetchPrimaryAccountInfoCompleted(
    ArcAuthService::RequestAccountInfoCallback callback,
    bool persistent_error,
    mojom::ArcAuthCodeStatus status,
    mojom::AccountInfoPtr account_info) {
  std::move(callback).Run(std::move(status), std::move(account_info),
                          persistent_error);
}

void CompleteFetchPrimaryAccountInfoWithMetrics(
    ArcAuthService::RequestPrimaryAccountInfoCallback callback,
    mojom::ArcAuthCodeStatus status,
    mojom::AccountInfoPtr account_info) {
  base::UmaHistogramEnumeration(
      kArcAuthRequestAccountInfoResultPrimaryHistogramName, status);
  std::move(callback).Run(std::move(status), std::move(account_info));
}

void CompleteFetchSecondaryAccountInfoWithMetrics(
    ArcAuthService::RequestAccountInfoCallback callback,
    mojom::ArcAuthCodeStatus status,
    mojom::AccountInfoPtr account_info,
    bool persistent_error) {
  base::UmaHistogramEnumeration(
      kArcAuthRequestAccountInfoResultSecondaryHistogramName, status);
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
      url_loader_factory_(profile_->GetDefaultStoragePartition()
                              ->GetURLLoaderFactoryForBrowserProcess()) {
  arc_bridge_service_->auth()->SetHost(this);
  arc_bridge_service_->auth()->AddObserver(this);

  ArcSessionManager::Get()->AddObserver(this);
  identity_manager_->AddObserver(this);

  if (ash::IsAccountManagerAvailable(profile_) && AreAccountsRestricted()) {
    account_apps_availability_ =
        ash::AccountAppsAvailabilityFactory::GetForProfile(profile_);

    account_apps_availability_->AddObserver(this);
  }
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
  // `TriggerAccountsPushToArc()` will not be triggered for the first session,
  // when ARC has not been provisioned yet. For the first session, an account
  // push will be triggered by `OnArcInitialStart()`, after a successful device
  // provisioning.
  // For the second and subsequent sessions, `arc::IsArcProvisioned()` will be
  // `true`.
  if (arc::IsArcProvisioned(profile_)) {
    TriggerAccountsPushToArc(false /* filter_primary_account */);
  }

  if (pending_get_arc_accounts_callback_) {
    DispatchAccountsInArc(std::move(pending_get_arc_accounts_callback_));
  }

  // Report main account resolution status for provisioned devices.
  if (!IsArcProvisioned(profile_)) {
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->auth(),
                                               GetMainAccountResolutionStatus);
  if (!instance) {
    return;
  }

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
  ArcProvisioningResult provisioning_result(std::move(result));

  if (account->is_initial_signin()) {
    // UMA for initial signin is updated from ArcSessionManager.
    ArcSessionManager::Get()->OnProvisioningFinished(provisioning_result);
    return;
  }

  // Re-auth shouldn't be triggered for non-Gaia device local accounts.
  if (!user_manager::UserManager::Get()->IsLoggedInAsUserWithGaiaAccount()) {
    NOTREACHED_IN_MIGRATION() << "Shouldn't re-auth for non-Gaia accounts";
    return;
  }

  const ProvisioningStatus status = GetProvisioningStatus(provisioning_result);

  if (!account->is_account_name() || !account->get_account_name() ||
      account->get_account_name().value().empty() ||
      IsPrimaryOrDeviceLocalAccount(identity_manager_,
                                    account->get_account_name().value())) {
    // Reauthorization for the Primary Account.
    // The check for |!account_name.has_value()| is for backwards compatibility
    // with older ARC versions, for which Mojo will set |account_name| to
    // empty/null.
    UpdateReauthorizationResultUMA(status, profile_);
  } else {
    UpdateSecondarySigninResultUMA(status);
  }
}

void ArcAuthService::ReportMetrics(mojom::MetricsType metrics_type,
                                   int32_t value) {
  switch (metrics_type) {
    case mojom::MetricsType::NETWORK_WAITING_TIME_MILLISECONDS:
      UpdateAuthTiming("Arc.Auth.NetworkWait.TimeDelta",
                       base::Milliseconds(value), profile_);
      break;
    case mojom::MetricsType::CHECKIN_ATTEMPTS:
      UpdateAuthCheckinAttempts(value, profile_);
      break;
    case mojom::MetricsType::CHECKIN_TIME_MILLISECONDS:
      UpdateAuthTiming("Arc.Auth.Checkin.TimeDelta", base::Milliseconds(value),
                       profile_);
      break;
    case mojom::MetricsType::SIGNIN_TIME_MILLISECONDS:
      UpdateAuthTiming("Arc.Auth.SignIn.TimeDelta", base::Milliseconds(value),
                       profile_);
      break;
    case mojom::MetricsType::ACCOUNT_CHECK_MILLISECONDS:
      UpdateAuthTiming("Arc.Auth.AccountCheck.TimeDelta",
                       base::Milliseconds(value), profile_);
      break;
  }
}

void ArcAuthService::ReportAccountCheckStatus(
    mojom::AccountCheckStatus status) {
  UpdateAuthAccountCheckStatus(status, profile_);
}

void ArcAuthService::ReportAccountReauthReason(mojom::ReauthReason reason) {
  UpdateAccountReauthReason(reason, profile_);
}

void ArcAuthService::ReportManagementChangeStatus(
    mojom::ManagementChangeStatus status) {
  UpdateSupervisionTransitionResultUMA(status);
  switch (status) {
    case mojom::ManagementChangeStatus::CLOUD_DPC_DISABLED:
    case mojom::ManagementChangeStatus::CLOUD_DPC_ALREADY_DISABLED:
    case mojom::ManagementChangeStatus::CLOUD_DPC_ENABLED:
    case mojom::ManagementChangeStatus::CLOUD_DPC_ALREADY_ENABLED:
      profile_->GetPrefs()->SetInteger(
          prefs::kArcManagementTransition,
          static_cast<int>(ArcManagementTransition::NO_TRANSITION));
      // TODO(brunokim): notify potential observers.
      break;
    case mojom::ManagementChangeStatus::CLOUD_DPC_DISABLING_FAILED:
    case mojom::ManagementChangeStatus::CLOUD_DPC_ENABLING_FAILED:
      LOG(ERROR) << "Management transition failed: " << status;
      ShowDataRemovalConfirmationDialog(
          profile_, base::BindOnce(&ArcAuthService::OnDataRemovalAccepted,
                                   weak_ptr_factory_.GetWeakPtr()));
      break;
    case mojom::ManagementChangeStatus::INVALID_MANAGEMENT_STATE:
      NOTREACHED_IN_MIGRATION()
          << "Invalid status of management transition: " << status;
  }
}

void ArcAuthService::RequestPrimaryAccountInfo(
    RequestPrimaryAccountInfoCallback callback) {
  // This is the provisioning flow.
  FetchPrimaryAccountInfo(
      true /* initial_signin */,
      base::BindOnce(&CompleteFetchPrimaryAccountInfoWithMetrics,
                     std::move(callback)));
}

void ArcAuthService::RequestAccountInfo(const std::string& account_name,
                                        RequestAccountInfoCallback callback) {
  // This is the post provisioning flow.
  // This request could have come for re-authenticating an existing account in
  // ARC, or for signing in a new Secondary Account.

  // Check if |account_name| points to a Secondary Account.
  if (!IsPrimaryOrDeviceLocalAccount(identity_manager_, account_name)) {
    FetchSecondaryAccountInfo(
        account_name,
        base::BindOnce(&CompleteFetchSecondaryAccountInfoWithMetrics,
                       std::move(callback)));
    return;
  }

  // TODO(solovey): Check secondary account ARC sign-in statistics and send
  // |persistent_error| == true for primary account for cases when refresh token
  // has persistent error.
  FetchPrimaryAccountInfo(
      false /* initial_signin */,
      base::BindOnce(
          &CompleteFetchPrimaryAccountInfoWithMetrics,
          base::BindOnce(&OnFetchPrimaryAccountInfoCompleted,
                         std::move(callback), false /* persistent_error */)));
}

void ArcAuthService::FetchPrimaryAccountInfo(
    bool initial_signin,
    RequestPrimaryAccountInfoCallback callback) {
  const mojom::ChromeAccountType account_type = GetAccountType(profile_);

  if (IsArcOptInVerificationDisabled()) {
    std::move(callback).Run(
        mojom::ArcAuthCodeStatus::SUCCESS,
        CreateAccountInfo(false /* is_enforced */,
                          std::string() /* auth_info */,
                          std::string() /* auth_name */, account_type,
                          policy_util::IsAccountManaged(profile_)));
    return;
  }

  if (account_type == mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT) {
    // Skip account auth code fetch for offline enrolled demo mode.
    std::move(callback).Run(
        mojom::ArcAuthCodeStatus::SUCCESS,
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
    DCHECK(identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin));
    auth_code_fetcher = CreateArcBackgroundAuthCodeFetcher(
        identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
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
  std::move(callback).Run(ash::IsAccountManagerAvailable(profile_));
}

void ArcAuthService::HandleAddAccountRequest() {
  DCHECK(ash::IsAccountManagerAvailable(profile_));

  ::GetAccountManagerFacade(profile_->GetPath().value())
      ->ShowAddAccountDialog(
          account_manager::AccountManagerFacade::AccountAdditionSource::kArc);
}

void ArcAuthService::HandleRemoveAccountRequest(const std::string& email) {
  DCHECK(ash::IsAccountManagerAvailable(profile_));

  // TODO(b/326488045) Update Settings path to kPeopleSectionPath when Settings
  // revamp is launched.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kMyAccountsSubpagePath);
}

void ArcAuthService::HandleUpdateCredentialsRequest(const std::string& email) {
  DCHECK(ash::IsAccountManagerAvailable(profile_));

  ::GetAccountManagerFacade(profile_->GetPath().value())
      ->ShowReauthAccountDialog(
          account_manager::AccountManagerFacade::AccountAdditionSource::kArc,
          email, base::DoNothing());
}

void ArcAuthService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // Should be consistent with OnAccountAvailableInArc.
  // TODO(crbug.com/40798532): Remove IdentityManager::Observer implementation.
  if (AreAccountsRestricted()) {
    return;
  }

  UpsertAccountToArc(account_info);
}

void ArcAuthService::OnExtendedAccountInfoRemoved(
    const AccountInfo& account_info) {
  // Should be consistent with OnAccountUnavailableInArc.
  // TODO(crbug.com/40798532): Remove IdentityManager::Observer implementation.
  if (AreAccountsRestricted()) {
    return;
  }

  DCHECK(!IsPrimaryGaiaAccount(account_info.gaia));

  RemoveAccountFromArc(account_info.email);
}

void ArcAuthService::OnAccountAvailableInArc(
    const account_manager::Account& account) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(AreAccountsRestricted());
  DCHECK(ash::IsAccountManagerAvailable(profile_));

  CoreAccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByEmailAddress(
          account.raw_email);
  // If account doesn't have a refresh token, `account_info` will be empty. In
  // this case `OnAccountAvailableInArc` will be called again after the refresh
  // token is loaded.
  if (account_info.IsEmpty()) {
    VLOG(1) << "Ignoring account update because CoreAccountInfo is empty for "
               "account: "
            << account.raw_email;
    return;
  }
  UpsertAccountToArc(account_info);
}

void ArcAuthService::OnAccountUnavailableInArc(
    const account_manager::Account& account) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(AreAccountsRestricted());
  DCHECK(ash::IsAccountManagerAvailable(profile_));

  DCHECK(!IsPrimaryGaiaAccount(account.key.id()));

  RemoveAccountFromArc(account.raw_email);
}

void ArcAuthService::OnArcInitialStart() {
  TriggerAccountsPushToArc(true /* filter_primary_account */);
}

void ArcAuthService::Shutdown() {
  identity_manager_->RemoveObserver(this);
  if (account_apps_availability_) {
    account_apps_availability_->RemoveObserver(this);
  }
}

void ArcAuthService::UpsertAccountToArc(const CoreAccountInfo& account_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!ash::IsAccountManagerAvailable(profile_)) {
    return;
  }

  // Ignore the update if ARC has not been provisioned yet.
  if (!arc::IsArcProvisioned(profile_)) {
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
  if (!instance) {
    return;
  }

  const std::string account_name = account_info.email;
  DCHECK(!account_name.empty());
  instance->OnAccountUpdated(account_name, mojom::AccountUpdateType::UPSERT);
}

void ArcAuthService::RemoveAccountFromArc(const std::string& email) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!ash::IsAccountManagerAvailable(profile_)) {
    return;
  }

  // Ignore the update if ARC has not been provisioned yet.
  if (!arc::IsArcProvisioned(profile_)) {
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->auth(),
                                               OnAccountUpdated);
  if (!instance) {
    return;
  }

  DCHECK(!email.empty());
  instance->OnAccountUpdated(email, mojom::AccountUpdateType::REMOVAL);
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
        mojom::ArcAuthCodeStatus::SUCCESS,
        CreateAccountInfo(!IsArcOptInVerificationDisabled(), auth_code,
                          full_account_id, GetAccountType(profile_),
                          policy_util::IsAccountManaged(profile_)));
  } else if (ash::DemoSession::Get() && ash::DemoSession::Get()->started()) {
    // For demo sessions, if auth code fetch failed (e.g. because the device is
    // offline), fall back to accountless offline demo mode provisioning.
    std::move(callback).Run(
        mojom::ArcAuthCodeStatus::SUCCESS,
        CreateAccountInfo(true /* is_enforced */, std::string() /* auth_info */,
                          std::string() /* auth_name */,
                          mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
                          true /* is_managed */));
  } else {
    // Send error to ARC.
    std::move(callback).Run(
        mojom::ArcAuthCodeStatus::CHROME_SERVER_COMMUNICATION_ERROR, nullptr);
  }
}

void ArcAuthService::FetchSecondaryAccountInfo(
    const std::string& account_name,
    RequestAccountInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByEmailAddress(account_name);
  if (account_info.IsEmpty()) {
    // Account is in ARC, but not in Chrome OS Account Manager.
    std::move(callback).Run(mojom::ArcAuthCodeStatus::CHROME_ACCOUNT_NOT_FOUND,
                            nullptr /* account_info */,
                            true /* persistent_error */);
    return;
  }

  const CoreAccountId& account_id = account_info.account_id;
  DCHECK(!account_id.empty());

  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id)) {
    std::move(callback).Run(
        mojom::ArcAuthCodeStatus::CHROME_SERVER_COMMUNICATION_ERROR,
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
        mojom::ArcAuthCodeStatus::SUCCESS,
        CreateAccountInfo(true /* is_enforced */, auth_code, account_name,
                          mojom::ChromeAccountType::USER_ACCOUNT,
                          false /* is_managed */),
        false /* persistent_error*/);
    return;
  }

  AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByEmailAddress(account_name);
  // Take care of the case when the user removes an account immediately after
  // adding/re-authenticating it.
  if (!account_info.IsEmpty()) {
    const bool is_persistent_error =
        identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info.account_id);
    std::move(callback).Run(
        mojom::ArcAuthCodeStatus::CHROME_SERVER_COMMUNICATION_ERROR,
        nullptr /* account_info */, is_persistent_error);
    return;
  }

  std::move(callback).Run(mojom::ArcAuthCodeStatus::CHROME_ACCOUNT_NOT_FOUND,
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
  NOTREACHED_IN_MIGRATION();
}

void ArcAuthService::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = std::move(url_loader_factory);
  url_loader_factory_for_testing_set_ = true;
}

void ArcAuthService::OnDataRemovalAccepted(bool accepted) {
  if (!accepted) {
    return;
  }
  if (!IsArcPlayStoreEnabledForProfile(profile_)) {
    return;
  }
  VLOG(1)
      << "Request for data removal on child transition failure is confirmed";
  ArcSessionManager::Get()->RequestArcDataRemoval();
  ArcSessionManager::Get()->StopAndEnableArc();
}

std::unique_ptr<ArcBackgroundAuthCodeFetcher>
ArcAuthService::CreateArcBackgroundAuthCodeFetcher(
    const CoreAccountId& account_id,
    bool initial_signin) {
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  DCHECK(!account_info.IsEmpty());
  auto fetcher = std::make_unique<ArcBackgroundAuthCodeFetcher>(
      url_loader_factory_, profile_, account_id, initial_signin,
      IsPrimaryGaiaAccount(account_info.gaia));

  return fetcher;
}

void ArcAuthService::TriggerAccountsPushToArc(bool filter_primary_account) {
  if (!ash::IsAccountManagerAvailable(profile_)) {
    return;
  }

  VLOG(1) << "Pushing accounts to ARC "
          << (filter_primary_account ? "without primary account"
                                     : "with primary account");
  if (AreAccountsRestricted()) {
    VLOG(1) << "Using AccountAppsAvailability to get available accounts";
    account_apps_availability_->GetAccountsAvailableInArc(
        base::BindOnce(&ArcAuthService::CompleteAccountsPushToArc,
                       weak_ptr_factory_.GetWeakPtr(), filter_primary_account));
    return;
  }

  const std::vector<CoreAccountInfo> accounts =
      identity_manager_->GetAccountsWithRefreshTokens();
  for (const CoreAccountInfo& account : accounts) {
    if (filter_primary_account && IsPrimaryGaiaAccount(account.gaia)) {
      continue;
    }

    OnRefreshTokenUpdatedForAccount(account);
  }
}

void ArcAuthService::CompleteAccountsPushToArc(
    bool filter_primary_account,
    const base::flat_set<account_manager::Account>& accounts) {
  DCHECK(AreAccountsRestricted());

  std::vector<mojom::ArcAccountInfoPtr> arc_accounts =
      std::vector<mojom::ArcAccountInfoPtr>();
  for (const auto& account : accounts) {
    DCHECK(account.key.account_type() == account_manager::AccountType::kGaia);
    if (filter_primary_account && IsPrimaryGaiaAccount(account.key.id())) {
      continue;
    }

    arc_accounts.emplace_back(mojom::ArcAccountInfo::New(
        /*email=*/account.raw_email, /*gaia_id=*/account.key.id()));
  }

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->auth(), SetAccounts);
  if (!instance) {
    VLOG(1) << "SetAccounts API is not available in ARC. Fallback to "
               "OnAccountAvailableInArc";
    for (const auto& account : accounts) {
      DCHECK(account.key.account_type() == account_manager::AccountType::kGaia);
      if (filter_primary_account && IsPrimaryGaiaAccount(account.key.id())) {
        continue;
      }

      OnAccountAvailableInArc(account);
    }
    return;
  }

  instance->SetAccounts(std::move(arc_accounts));
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

// static
void ArcAuthService::EnsureFactoryBuilt() {
  ArcAuthServiceFactory::GetInstance();
}

bool ArcAuthService::AreAccountsRestricted() {
  return ash::AccountAppsAvailability::IsArcAccountRestrictionsEnabled() ||
         ash::AccountAppsAvailability::IsArcManagedAccountRestrictionEnabled();
}

}  // namespace arc
