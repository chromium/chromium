// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"

#include <memory>
#include <set>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/users/affiliation.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/policy_oauth2_token_fetcher.h"
#include "chrome/browser/ash/policy/login/wildcard_login_checker.h"
#include "chrome/browser/ash/policy/remote_commands/user_commands_factory_ash.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log_uploader.h"
#include "chrome/browser/ash/policy/skyvault/local_files_cleanup.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/reporting/report_scheduler_desktop.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/cloud/user_fm_registration_token_uploader_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"
#include "components/enterprise/browser/reporting/report_generator.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/policy/core/common/cloud/affiliation.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/remote_commands/remote_commands_invalidator_impl.h"
#include "components/policy/policy_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// UMA histogram names.
const char kUMAInitialFetchOAuth2Error[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.OAuth2Error";
const char kUMAReregistrationResult[] =
    "Enterprise.UserPolicyChromeOS.ReregistrationResult";

// The oauth token consumer name.
const char kOAuthConsumerName[] = "user_cloud_policy_manager_ash";

// This enum is used in UMA, items should not be reordered/deleted. New values
// should also be added to enums.xml.
enum class RegistrationResult {
  kReregistrationTriggered = 0,
  kReregistrationSuccessful = 1,
  kReregistrationUnsuccessful = 2,
  kMaxValue = kReregistrationUnsuccessful,
};

void RegistrationResultUMA(RegistrationResult registration_result) {
  UMA_HISTOGRAM_ENUMERATION(kUMAReregistrationResult, registration_result);
}

// Returns whether user with |account_id| is a child. Returns false if user with
// |account_id| is not found.
bool IsChildUser(const AccountId& account_id) {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  return user && user->GetType() == user_manager::UserType::kChild;
}

// This class is used to subscribe for notifications that the current profile is
// being shut down.
class UserCloudPolicyManagerAshNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static UserCloudPolicyManagerAshNotifierFactory* GetInstance() {
    return base::Singleton<UserCloudPolicyManagerAshNotifierFactory>::get();
  }

  UserCloudPolicyManagerAshNotifierFactory(
      const UserCloudPolicyManagerAshNotifierFactory&) = delete;
  UserCloudPolicyManagerAshNotifierFactory& operator=(
      const UserCloudPolicyManagerAshNotifierFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      UserCloudPolicyManagerAshNotifierFactory>;

  UserCloudPolicyManagerAshNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "UserRemoteCommandsInvalidator") {
    DependsOn(invalidation::ProfileInvalidationProviderFactory::GetInstance());
    DependsOn(policy::UserFmRegistrationTokenUploaderFactory::GetInstance());
  }

  ~UserCloudPolicyManagerAshNotifierFactory() override = default;
};

// Returns true only if SkyVault TT is enabled, but GA is not.
bool IsSkyVaultTTEnabled() {
  return base::FeatureList::IsEnabled(features::kSkyVault) &&
         !base::FeatureList::IsEnabled(features::kSkyVaultV2);
}

}  // namespace

UserCloudPolicyManagerAsh::UserCloudPolicyManagerAsh(
    Profile* profile,
    std::unique_ptr<CloudPolicyStore> store,
    std::unique_ptr<CloudExternalDataManager> external_data_manager,
    const base::FilePath& component_policy_cache_path,
    PolicyEnforcement enforcement_type,
    PrefService* local_state,
    base::TimeDelta policy_refresh_timeout,
    base::OnceClosure fatal_error_callback,
    const AccountId& account_id,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : CloudPolicyManager(
          dm_protocol::kChromeUserPolicyType,
          std::string(),
          std::move(store),
          task_runner,
          base::BindRepeating(content::GetNetworkConnectionTracker)),
      profile_(profile),
      external_data_manager_(std::move(external_data_manager)),
      component_policy_cache_path_(component_policy_cache_path),
      waiting_for_policy_fetch_(enforcement_type ==
                                    PolicyEnforcement::kServerCheckRequired ||
                                !policy_refresh_timeout.is_zero()),
      enforcement_type_(enforcement_type),
      local_state_(local_state),
      account_id_(account_id),
      fatal_error_callback_(std::move(fatal_error_callback)) {
  DCHECK(profile_);
  DCHECK(local_state_);

  // If a refresh timeout was specified, set a timer to call us back.
  if (!policy_refresh_timeout.is_zero()) {
    // Shouldn't pass a timeout unless we're refreshing existing policy.
    DCHECK_EQ(enforcement_type_, PolicyEnforcement::kPolicyRequired);
    policy_refresh_timeout_.Start(
        FROM_HERE, policy_refresh_timeout,
        base::BindOnce(&UserCloudPolicyManagerAsh::OnPolicyRefreshTimeout,
                       base::Unretained(this)));
  }

  // Register for notification that profile creation is complete - this is used
  // for creating the invalidator for user remote commands. The invalidator must
  // not be initialized before then because the invalidation service cannot be
  // started because it depends on components initialized at the end of profile
  // creation. https://crbug.com/171406
  observed_profile_.Observe(profile_.get());
}

void UserCloudPolicyManagerAsh::ForceTimeoutForTest() {
  DCHECK(policy_refresh_timeout_.IsRunning());
  // Stop the timer to mimic what happens when a real timer fires, then invoke
  // the timer callback directly.
  policy_refresh_timeout_.Stop();
  OnPolicyRefreshTimeout();
}

void UserCloudPolicyManagerAsh::SetSignInURLLoaderFactoryForTests(
    scoped_refptr<network::SharedURLLoaderFactory> signin_url_loader_factory) {
  signin_url_loader_factory_for_tests_ = signin_url_loader_factory;
}

void UserCloudPolicyManagerAsh::SetSystemURLLoaderFactoryForTests(
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory) {
  system_url_loader_factory_for_tests_ = system_url_loader_factory;
}

UserCloudPolicyManagerAsh::~UserCloudPolicyManagerAsh() = default;

void UserCloudPolicyManagerAsh::ConnectManagementService(
    DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory) {
  DCHECK(device_management_service);

  CHECK(!core()->client());

  // Note: |system_url_loader_factory| can be null for tests.
  // Use the system URL loader context here instead of a context derived
  // from the Profile because Connect() is called before the profile is
  // fully initialized (required so we can perform the initial policy load).
  std::unique_ptr<CloudPolicyClient> cloud_policy_client =
      std::make_unique<CloudPolicyClient>(
          device_management_service, system_url_loader_factory,
          ash::GetDeviceDMTokenForUserPolicyGetter(account_id_));
  CreateComponentCloudPolicyService(
      dm_protocol::kChromeExtensionPolicyType, component_policy_cache_path_,
      cloud_policy_client.get(), schema_registry());
  core()->Connect(std::move(cloud_policy_client));
  client()->AddObserver(this);

  external_data_manager_->Connect(system_url_loader_factory);

  // Determine the next step after the CloudPolicyService initializes.
  if (service()->IsInitializationComplete()) {
    // The CloudPolicyStore is already initialized here, which means we must
    // have done a synchronous load of policy - this only happens after a crash
    // and restart. If we crashed and restarted, it's not possible to block
    // waiting for a policy fetch (profile is loading synchronously and async
    // operations can't be handled).

    // If we are doing a synchronous load, then wait_for_policy_fetch_ should
    // never be set (because we can't wait).
    CHECK(!waiting_for_policy_fetch_);
    if (!client()->is_registered() &&
        enforcement_type_ != PolicyEnforcement::kPolicyOptional) {
      // We expected to load policy, but we don't have policy, so exit the
      // session.
      LOG(ERROR) << "Failed to load policy during synchronous restart "
                 << "- terminating session";
      if (fatal_error_callback_) {
        std::move(fatal_error_callback_).Run();
      }
      return;
    }

    // Initialization has completed before our observer was registered
    // so invoke our callback directly.
    OnCloudPolicyServiceInitializationCompleted();
  } else {
    // Wait for the CloudPolicyStore to finish initializing.
    service()->AddObserver(this);
  }

  app_install_event_log_uploader_ =
      std::make_unique<ArcAppInstallEventLogUploader>(client(), profile_);

  if (IsSkyVaultTTEnabled()) {
    // Local files should be deleted if required by policy.
    local_files_cleanup_ =
        std::make_unique<local_user_files::LocalFilesCleanup>();
  }
}

void UserCloudPolicyManagerAsh::OnAccessTokenAvailable(
    const std::string& access_token) {
  // This method should be called only once (at the beginning of the session)
  // for regular user.
  // For child user this method will be called multiple times (periodically).

  access_token_ = access_token;

  if (!wildcard_username_.empty()) {
    wildcard_login_checker_ = std::make_unique<WildcardLoginChecker>();
    // Safe to set a callback with an unretained pointer because the
    // WildcardLoginChecker is owned by this object and won't invoke the
    // callback after we destroy it.
    wildcard_login_checker_->StartWithAccessToken(
        access_token,
        base::BindOnce(&UserCloudPolicyManagerAsh::OnWildcardCheckCompleted,
                       base::Unretained(this), wildcard_username_));
  }

  if (service() && service()->IsInitializationComplete() && client()) {
    if (!client()->is_registered()) {
      OnOAuth2PolicyTokenFetched(
          access_token, GoogleServiceAuthError(GoogleServiceAuthError::NONE));
    } else if (RequiresOAuthTokenForChildUser()) {
      client()->SetOAuthTokenAsAdditionalAuth(access_token);
      StartRefreshSchedulerIfReady();
    }
  }
}

bool UserCloudPolicyManagerAsh::RequiresOAuthTokenForChildUser() const {
  return IsChildUser(account_id_) &&
         base::FeatureList::IsEnabled(features::kDMServerOAuthForChildUser);
}

void UserCloudPolicyManagerAsh::OnWildcardCheckCompleted(
    const std::string& username,
    WildcardLoginChecker::Result result) {
  if (result == WildcardLoginChecker::RESULT_BLOCKED) {
    LOG(ERROR) << "Online wildcard login check failed, terminating session.";

    // TODO(mnissler): This only removes the user pod from the login screen, but
    // the cryptohome remains. This is because deleting the cryptohome for a
    // logged-in session is not possible. Fix this either by delaying the
    // cryptohome deletion operation or by getting rid of the in-session
    // wildcard check.
    // Also note that, following |fatal_error_callback_| is practically
    // OnUserPolicyFatalError above, so is attempting to shutting down Chrome.
    // Thus, some asynchronous operations such as reporting in
    // UserAddedRemovedReporter are not guaranteed to be completed unless
    // task runners' priority/shutdown-behavior are configured.
    user_manager::UserManager::Get()->RemoveUserFromList(
        AccountId::FromUserEmail(username));
    if (fatal_error_callback_) {
      std::move(fatal_error_callback_).Run();
    }
  }
}

void UserCloudPolicyManagerAsh::EnableWildcardLoginCheck(
    const std::string& username) {
  DCHECK(access_token_.empty());
  wildcard_username_ = username;
}

ArcAppInstallEventLogUploader*
UserCloudPolicyManagerAsh::GetAppInstallEventLogUploader() {
  return app_install_event_log_uploader_.get();
}

void UserCloudPolicyManagerAsh::Shutdown() {
  observed_profile_.Reset();
  local_files_cleanup_.reset();
  app_install_event_log_uploader_.reset();
  report_scheduler_.reset();
  if (client()) {
    client()->RemoveObserver(this);
  }
  if (service()) {
    service()->RemoveObserver(this);
  }
  token_fetcher_.reset();
  external_data_manager_->Disconnect();
  CloudPolicyManager::Shutdown();
}

bool UserCloudPolicyManagerAsh::IsInitializationComplete(
    PolicyDomain domain) const {
  if (!CloudPolicyManager::IsInitializationComplete(domain)) {
    return false;
  }
  if (domain == POLICY_DOMAIN_CHROME) {
    return !waiting_for_policy_fetch_;
  }
  return true;
}

void UserCloudPolicyManagerAsh::OnCloudPolicyServiceInitializationCompleted() {
  service()->RemoveObserver(this);

  // If the CloudPolicyClient isn't registered at this stage then it needs an
  // OAuth token for the initial registration (there's no cached policy).
  //
  // If |waiting_for_policy_fetch_| is true then Profile initialization
  // is blocking on the initial policy fetch, so the token must be fetched
  // immediately. In that case, the signin Profile is used to authenticate a
  // Gaia request to fetch a refresh token, and then the policy token is
  // fetched.
  //
  // If |waiting_for_policy_fetch_| is false (meaning this is a
  // pre-existing session that doesn't have policy and we're just doing a
  // background check to see if the user has become managed since last signin)
  // then the UserCloudPolicyTokenForwarder service will eventually call
  // OnAccessTokenAvailable() once an access token is available. That call may
  // have already happened while waiting for initialization of the
  // CloudPolicyService, so in that case check if an access token is already
  // available.
  if (!client()->is_registered()) {
    if (waiting_for_policy_fetch_) {
      FetchPolicyOAuthToken();
    } else if (!access_token_.empty()) {
      OnAccessTokenAvailable(access_token_);
    }
  }

  // If this isn't blocking on a policy fetch then
  // CloudPolicyManager::OnStoreLoaded() already published the cached policy.
  // Start the refresh scheduler now, which will eventually refresh the
  // cached policy or make the first fetch once the OAuth2 token is
  // available. If refresh scheduler is already started this call will do
  // nothing.
  StartRefreshSchedulerIfReady();

  // Start the report scheduler to periodically upload usage data to DM server.
  StartReportSchedulerIfReady(true /* enable_delayed_creation */);
}

void UserCloudPolicyManagerAsh::OnPolicyFetched(CloudPolicyClient* client) {
  // No action required. If we're blocked on a policy fetch, we'll learn about
  // completion of it through OnInitialPolicyFetchComplete().
}

void UserCloudPolicyManagerAsh::OnRegistrationStateChanged(
    CloudPolicyClient* cloud_policy_client) {
  DCHECK_EQ(client(), cloud_policy_client);

  // Trigger re-registration. This happens if the client ID used for policy
  // fetches is unknown/purged from the DMServer.
  if (!client()->is_registered() && client()->requires_reregistration()) {
    RegistrationResultUMA(RegistrationResult::kReregistrationTriggered);
    is_in_reregistration_state_ = true;
    if (!access_token_.empty()) {
      OnOAuth2PolicyTokenFetched(
          access_token_, GoogleServiceAuthError(GoogleServiceAuthError::NONE));
    } else {
      FetchPolicyOAuthToken();
    }
    return;
  }
  // Reset re-registration state on successful registration.
  if (client()->is_registered() && is_in_reregistration_state_) {
    RegistrationResultUMA(RegistrationResult::kReregistrationSuccessful);
    is_in_reregistration_state_ = false;
  }

  if (waiting_for_policy_fetch_) {
    // If we're blocked on the policy fetch, now is a good time to issue it.
    if (client()->is_registered()) {
      service()->RefreshPolicy(
          base::BindOnce(
              &UserCloudPolicyManagerAsh::OnInitialPolicyFetchComplete,
              base::Unretained(this)),
          PolicyFetchReason::kRegistrationChanged);
    } else {
      // If the client has switched to not registered, we bail out as this
      // indicates the cloud policy setup flow has been aborted.
      CancelWaitForPolicyFetch(true, std::string());
    }
  }
}

void UserCloudPolicyManagerAsh::OnClientError(
    CloudPolicyClient* cloud_policy_client) {
  DCHECK_EQ(client(), cloud_policy_client);
  switch (client()->last_dm_status()) {
    case DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
      // If management is not supported for this user, then a registration
      // error is to be expected - treat as a policy fetch success. Also
      // mark this profile as not requiring policy.
      SetPolicyRequired(false);
      CancelWaitForPolicyFetch(true, std::string());
      break;
    case DM_STATUS_SUCCESS:
      CancelWaitForPolicyFetch(true, std::string());
      break;
    default:
      // Unexpected error fetching policy.
      CancelWaitForPolicyFetch(
          false, "cloud policy client status: " +
                     base::NumberToString(client()->last_dm_status()));
      break;
  }
  // If we are in re-registration state and re-registration fails, we mark the
  // user to require an online sign-in on his next sign-in.
  if (is_in_reregistration_state_) {
    RegistrationResultUMA(RegistrationResult::kReregistrationUnsuccessful);
    LOG(ERROR) << "Re-registration failed, requiring the user to perform an "
                  "online sign-in.";
    user_manager::UserManager::Get()->SaveForceOnlineSignin(account_id_, true);
  }
}

void UserCloudPolicyManagerAsh::OnComponentCloudPolicyUpdated() {
  CloudPolicyManager::OnComponentCloudPolicyUpdated();
  StartRefreshSchedulerIfReady();
}

void UserCloudPolicyManagerAsh::OnUserProfileLoaded(
    const AccountId& account_id) {
  if (!user_manager::UserManager::Get()) {
    return;
  }

  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();

  if (!primary_user || primary_user->GetAccountId() != account_id ||
      !primary_user->is_profile_created()) {
    return;
  }

  session_manager::SessionManager::Get()->RemoveObserver(this);
  StartReportSchedulerIfReady(false /* enable_delayed_creation */);
}

void UserCloudPolicyManagerAsh::OnStoreLoaded(
    CloudPolicyStore* cloud_policy_store) {
  CloudPolicyManager::OnStoreLoaded(cloud_policy_store);

  em::PolicyData const* const policy_data = cloud_policy_store->policy();

  if (policy_data) {
    // We have cached policy in the store, so update the various flags to
    // reflect that we have policy.
    SetPolicyRequired(true);

    // Policy was successfully loaded from disk, so it's OK if a subsequent
    // server fetch fails.
    enforcement_type_ = PolicyEnforcement::kPolicyOptional;

    DCHECK(policy_data->has_username());

    policy::BrowserPolicyConnectorAsh const* const connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    const bool is_affiliated = policy::IsUserAffiliated(
        base::flat_set<std::string>(policy_data->user_affiliation_ids().begin(),
                                    policy_data->user_affiliation_ids().end()),
        connector->device_affiliation_ids(), account_id_.GetUserEmail());

    user_manager::UserManager::Get()->SetUserAffiliated(account_id_,
                                                        is_affiliated);
  }
}

void UserCloudPolicyManagerAsh::SetPolicyRequired(bool policy_required) {
  auto* user_manager = user_manager::UserManager::Get();
  user_manager::KnownUser known_user(local_state_);
  known_user.SetProfileRequiresPolicy(
      account_id_,
      policy_required ? user_manager::ProfileRequiresPolicy::kPolicyRequired
                      : user_manager::ProfileRequiresPolicy::kNoPolicyRequired);
  if (user_manager->IsCurrentUserNonCryptohomeDataEphemeral()) {
    // For ephemeral users, we need to set a flag via session manager - this
    // handles the case where the session restarts due to a crash (the restarted
    // instance will know whether policy is required via this flag). This
    // overwrites flags set by about://flags, but that's OK since we can't have
    // any of those flags set at startup anyway for ephemeral sessions.
    base::CommandLine command_line =
        base::CommandLine(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(ash::switches::kProfileRequiresPolicy,
                                   policy_required ? "true" : "false");
    base::CommandLine::StringVector flags;
    flags.assign(command_line.argv().begin() + 1, command_line.argv().end());
    DCHECK_EQ(1u, flags.size());
    ash::UserSessionManager::GetInstance()->SetSwitchesForUser(
        account_id_,
        ash::UserSessionManager::CommandLineSwitchesType::kSessionControl,
        flags);
  }
}

void UserCloudPolicyManagerAsh::GetChromePolicy(PolicyMap* policy_map) {
  CloudPolicyManager::GetChromePolicy(policy_map);

  // If the store has a verified policy blob received from the server then apply
  // the defaults for policies that haven't been configured by the administrator
  // given that this is an enterprise user.
  if (!store()->has_policy()) {
    return;
  }

  // Don't apply enterprise defaults for Child user.
  if (IsChildUser(account_id_)) {
    return;
  }

  SetEnterpriseUsersDefaults(policy_map);
}

void UserCloudPolicyManagerAsh::FetchPolicyOAuthToken() {
  // By-pass token fetching for test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kDisableGaiaServices)) {
    OnOAuth2PolicyTokenFetched(
        "fake_policy_token",
        GoogleServiceAuthError(GoogleServiceAuthError::NONE));
    return;
  }

  // TODO(jcivelli): Connect() is passed a SharedURLLoaderFactory but here we
  // retrieve it from |g_browser_process|. We should move away from retrieving
  // it from |g_browser_process| at which point we can remove
  // SetSystemURLLoaderFactoryForTests().
  scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory =
      system_url_loader_factory_for_tests_;
  if (!system_url_loader_factory) {
    system_url_loader_factory =
        g_browser_process->system_network_context_manager()
            ->GetSharedURLLoaderFactory();
  }

  std::string refresh_token = user_context_refresh_token_for_tests_.value_or(
      ash::UserSessionManager::GetInstance()->user_context().GetRefreshToken());

  if (!refresh_token.empty()) {
    token_fetcher_ =
        PolicyOAuth2TokenFetcher::CreateInstance(kOAuthConsumerName);
    token_fetcher_->StartWithRefreshToken(
        refresh_token, system_url_loader_factory,
        base::BindOnce(&UserCloudPolicyManagerAsh::OnOAuth2PolicyTokenFetched,
                       base::Unretained(this)));
    return;
  }

  LOG(ERROR) << "No refresh token for policy oauth token fetch!";
  OnOAuth2PolicyTokenFetched(
      std::string(),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

void UserCloudPolicyManagerAsh::OnOAuth2PolicyTokenFetched(
    const std::string& policy_token,
    const GoogleServiceAuthError& error) {
  DCHECK(!client()->is_registered());

  if (error.state() == GoogleServiceAuthError::NONE) {
    if (RequiresOAuthTokenForChildUser()) {
      client()->SetOAuthTokenAsAdditionalAuth(policy_token);
    }

    // Start client registration. Either OnRegistrationStateChanged() or
    // OnClientError() will be called back.
    CloudPolicyClient::RegistrationParameters parameters(
        em::DeviceRegisterRequest::USER,
        em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
    if (user_manager::UserManager::Get()
            ->IsCurrentUserCryptohomeDataEphemeral()) {
      parameters.lifetime = em::DeviceRegisterRequest::LIFETIME_EPHEMERAL_USER;
    }
    std::string client_id;
    if (client()->requires_reregistration()) {
      client_id = client()->client_id();
    }
    client()->Register(parameters, client_id, policy_token);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kUMAInitialFetchOAuth2Error, error.state(),
                              GoogleServiceAuthError::NUM_STATES);
    // Failed to get a token, stop waiting if policy is not required for this
    // user.
    CancelWaitForPolicyFetch(
        false, "auth error state: " + base::NumberToString(error.state()));
  }

  token_fetcher_.reset();
}

void UserCloudPolicyManagerAsh::OnInitialPolicyFetchComplete(bool success) {
  CancelWaitForPolicyFetch(
      success,
      "policy fetch complete"
      ", policy client status: " +
          base::NumberToString(client()->last_dm_status()) +
          ", store status: " + base::NumberToString(store()->status()));
}

void UserCloudPolicyManagerAsh::OnPolicyRefreshTimeout() {
  DCHECK(waiting_for_policy_fetch_);
  LOG(WARNING) << "Timed out while waiting for the policy refresh. "
               << "The session will start with the cached policy.";
  CancelWaitForPolicyFetch(false, "policy refresh timeout");
}

void UserCloudPolicyManagerAsh::CancelWaitForPolicyFetch(
    bool success,
    const std::string& failure_reason) {
  if (!waiting_for_policy_fetch_) {
    return;
  }

  policy_refresh_timeout_.Stop();

  // If there was an error, and we don't want to allow profile initialization
  // to go forward after a failed policy fetch, then trigger a fatal error.
  if (!success && enforcement_type_ != PolicyEnforcement::kPolicyOptional) {
    LOG(ERROR) << "Policy fetch failed for the user. "
                  "Aborting profile initialization. "
               << failure_reason;
    // Also log to syslog so the message is visible even when the user home is
    // unmounted.
    SYSLOG(ERROR) << "Policy fetching failed. " << failure_reason;
    // Need to exit the current user, because we've already started this user's
    // session.
    if (fatal_error_callback_) {
      std::move(fatal_error_callback_).Run();
    }
    return;
  }

  waiting_for_policy_fetch_ = false;

  CheckAndPublishPolicy();
  // Now that |waiting_for_policy_fetch_| is guaranteed to be false, the
  // scheduler can be started.
  StartRefreshSchedulerIfReady();
}

void UserCloudPolicyManagerAsh::StartRefreshSchedulerIfReady() {
  if (core()->refresh_scheduler()) {
    return;  // Already started.
  }

  if (waiting_for_policy_fetch_) {
    return;  // Still waiting for the initial, blocking fetch.
  }

  if (!service() || !local_state_) {
    return;  // Not connected.
  }

  if (component_policy_service() &&
      !component_policy_service()->is_initialized()) {
    // If the client doesn't have the list of components to fetch yet then don't
    // start the scheduler. The |component_policy_service_| will call back into
    // OnComponentCloudPolicyUpdated() once it's ready.
    return;
  }

  // Do not start refresh scheduler until OAuth token is available for child
  // user, because policy refresh will fail.
  if (RequiresOAuthTokenForChildUser() && access_token_.empty()) {
    return;
  }

  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state_,
                                policy_prefs::kUserPolicyRefreshRate);
}

void UserCloudPolicyManagerAsh::StartReportSchedulerIfReady(
    bool enable_delayed_creation) {
  if (!client() || !client()->is_registered()) {
    return;
  }

  if (!user_manager::UserManager::Get()) {
    return;
  }

  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();

  if (!primary_user) {
    return;
  }

  // SessionManager uses another thread to load profiles. If the operation
  // doesn't finish, the creation of |report_scheduler_| will be delayed and
  // relies on SessionManagerObserver methods to monitor the progress.
  if (enable_delayed_creation && !primary_user->is_profile_created()) {
    session_manager::SessionManager::Get()->AddObserver(this);
    VLOG(0) << "Report scheduler is delayed to create because the primary "
               "user profile hasn't been loaded. This is a designed behavior.";
    return;
  }

  // TODO(crbug.com/40703888): Split up Chrome OS reporting code into its own
  // delegates, then use the Chrome OS delegate factory here.
  enterprise_reporting::ReportingDelegateFactoryDesktop delegate_factory;
  enterprise_reporting::ReportScheduler::CreateParams params;
  params.client = client();
  params.delegate = delegate_factory.GetReportSchedulerDelegate();
  params.report_generator =
      std::make_unique<enterprise_reporting::ReportGenerator>(
          &delegate_factory);

  delegate_factory.SetProfileForRealTimeController(profile_);
  params.real_time_report_controller =
      std::make_unique<enterprise_reporting::RealTimeReportController>(
          &delegate_factory);

  report_scheduler_ = std::make_unique<enterprise_reporting::ReportScheduler>(
      std::move(params));

  report_scheduler_->OnDMTokenUpdated();
}

void UserCloudPolicyManagerAsh::OnProfileInitializationComplete(
    Profile* profile) {
  DCHECK(observed_profile_.IsObservingSource(profile));
  observed_profile_.Reset();

  // Activate user remote commands only for unicorn accounts.
  // The server side only supports user-scoped remote commands for unicorn
  // accounts at the moment. See b/193450869 for more detail.
  if (!IsChildUser(account_id_)) {
    return;
  }

  invalidation::ProfileInvalidationProvider* const invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile_);

  if (!invalidation_provider) {
    return;
  }

  core()->StartRemoteCommandsService(
      std::make_unique<UserCommandsFactoryAsh>(profile_),
      PolicyInvalidationScope::kUser);
  invalidator_ = std::make_unique<RemoteCommandsInvalidatorImpl>(
      core(), base::DefaultClock::GetInstance(),
      PolicyInvalidationScope::kUser);

  invalidator_->Initialize(
      invalidation_provider->GetInvalidationServiceOrListener(
          kPolicyFCMInvalidationSenderID,
          invalidation::InvalidationListener::kProjectNumberEnterprise));

  shutdown_subscription_ =
      UserCloudPolicyManagerAshNotifierFactory::GetInstance()
          ->Get(profile_)
          ->Subscribe(base::BindRepeating(
              &UserCloudPolicyManagerAsh::ShutdownRemoteCommands,
              base::Unretained(this)));
}

void UserCloudPolicyManagerAsh::ShutdownRemoteCommands() {
  // Unregister the RemoteCommandsInvalidatorImpl from the InvalidatorRegistrar.
  invalidator_->Shutdown();
  invalidator_.reset();
  shutdown_subscription_ = {};
}

void UserCloudPolicyManagerAsh::SetUserContextRefreshTokenForTests(
    const std::string& refresh_token) {
  DCHECK(!refresh_token.empty());
  DCHECK(!user_context_refresh_token_for_tests_);
  user_context_refresh_token_for_tests_ = std::make_optional(refresh_token);
}

enterprise_reporting::ReportScheduler*
UserCloudPolicyManagerAsh::GetReportSchedulerForTesting() {
  return report_scheduler_.get();
}

// static
void UserCloudPolicyManagerAsh::EnsureFactoryBuilt() {
  UserCloudPolicyManagerAshNotifierFactory::GetInstance();
}

std::string_view UserCloudPolicyManagerAsh::name() const {
  return "UserCloudPolicyManagerAsh";
}

}  // namespace policy
