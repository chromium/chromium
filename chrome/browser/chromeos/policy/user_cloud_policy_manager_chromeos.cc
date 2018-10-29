// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/users/affiliation.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_uploader.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"
#include "chrome/browser/chromeos/policy/remote_commands/user_commands_factory_chromeos.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/policy/wildcard_login_checker.h"
#include "chrome/browser/invalidation/deprecated_profile_invalidation_provider_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/cloud/remote_commands_invalidator_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chromeos/chromeos_switches.h"
#include "components/crash/core/common/crash_key.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// UMA histogram names.
const char kUMADelayInitialization[] =
    "Enterprise.UserPolicyChromeOS.DelayInitialization";
const char kUMAInitialFetchClientError[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.ClientError";
const char kUMAInitialFetchDelayClientRegister[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.DelayClientRegister";
const char kUMAInitialFetchDelayOAuth2Token[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.DelayOAuth2Token";
const char kUMAInitialFetchDelayPolicyFetch[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.DelayPolicyFetch";
const char kUMAInitialFetchDelayTotal[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.DelayTotal";
const char kUMAInitialFetchOAuth2Error[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.OAuth2Error";
const char kUMAInitialFetchOAuth2NetworkError[] =
    "Enterprise.UserPolicyChromeOS.InitialFetch.OAuth2NetworkError";
const char kUMAReregistrationResult[] =
    "Enterprise.UserPolicyChromeOS.ReregistrationResult";

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

// This class is used to subscribe for notifications that the current profile is
// being shut down.
class UserCloudPolicyManagerChromeOSNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static UserCloudPolicyManagerChromeOSNotifierFactory* GetInstance() {
    return base::Singleton<
        UserCloudPolicyManagerChromeOSNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<
      UserCloudPolicyManagerChromeOSNotifierFactory>;

  UserCloudPolicyManagerChromeOSNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "UserRemoteCommandsInvalidator") {
    DependsOn(invalidation::DeprecatedProfileInvalidationProviderFactory::
                  GetInstance());
  }

  ~UserCloudPolicyManagerChromeOSNotifierFactory() override = default;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerChromeOSNotifierFactory);
};

}  // namespace

UserCloudPolicyManagerChromeOS::UserCloudPolicyManagerChromeOS(
    Profile* profile,
    std::unique_ptr<CloudPolicyStore> store,
    std::unique_ptr<CloudExternalDataManager> external_data_manager,
    const base::FilePath& component_policy_cache_path,
    PolicyEnforcement enforcement_type,
    base::TimeDelta policy_refresh_timeout,
    base::OnceClosure fatal_error_callback,
    const AccountId& account_id,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : CloudPolicyManager(
          dm_protocol::kChromeUserPolicyType,
          std::string(),
          store.get(),
          task_runner,
          base::BindRepeating(content::GetNetworkConnectionTracker)),
      profile_(profile),
      store_(std::move(store)),
      external_data_manager_(std::move(external_data_manager)),
      component_policy_cache_path_(component_policy_cache_path),
      waiting_for_policy_fetch_(enforcement_type ==
                                    PolicyEnforcement::kServerCheckRequired ||
                                !policy_refresh_timeout.is_zero()),
      enforcement_type_(enforcement_type),
      account_id_(account_id),
      fatal_error_callback_(std::move(fatal_error_callback)) {
  DCHECK(profile_);
  time_init_started_ = base::Time::Now();

  // Some tests don't want to complete policy initialization until they have
  // manually injected policy even though the profile itself is synchronously
  // initialized.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kWaitForInitialPolicyFetchForTest)) {
    waiting_for_policy_fetch_ = true;
  }

  // If a refresh timeout was specified, set a timer to call us back.
  if (!policy_refresh_timeout.is_zero()) {
    // Shouldn't pass a timeout unless we're refreshing existing policy.
    DCHECK_EQ(enforcement_type_, PolicyEnforcement::kPolicyRequired);
    policy_refresh_timeout_.Start(
        FROM_HERE, policy_refresh_timeout,
        base::BindRepeating(
            &UserCloudPolicyManagerChromeOS::OnPolicyRefreshTimeout,
            base::Unretained(this)));
  }

  // Register for notification that profile creation is complete - this is used
  // for creating the invalidator for user remote commands. The invalidator must
  // not be initialized before then because the invalidation service cannot be
  // started because it depends on components initialized at the end of profile
  // creation.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
                 content::Source<Profile>(profile));
}

void UserCloudPolicyManagerChromeOS::ForceTimeoutForTest() {
  DCHECK(policy_refresh_timeout_.IsRunning());
  // Stop the timer to mimic what happens when a real timer fires, then invoke
  // the timer callback directly.
  policy_refresh_timeout_.Stop();
  OnPolicyRefreshTimeout();
}

void UserCloudPolicyManagerChromeOS::SetSignInURLLoaderFactoryForTests(
    scoped_refptr<network::SharedURLLoaderFactory> signin_url_loader_factory) {
  signin_url_loader_factory_for_tests_ = signin_url_loader_factory;
}

void UserCloudPolicyManagerChromeOS::SetSystemURLLoaderFactoryForTests(
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory) {
  system_url_loader_factory_for_tests_ = system_url_loader_factory;
}

UserCloudPolicyManagerChromeOS::~UserCloudPolicyManagerChromeOS() {}

void UserCloudPolicyManagerChromeOS::Connect(
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory) {
  DCHECK(device_management_service);
  DCHECK(local_state);

  // TODO(emaxx): Remove the crash key after the crashes tracked at
  // https://crbug.com/685996 are fixed.
  if (core()->client()) {
    static crash_reporter::CrashKeyString<1024> connect_callstack_key(
        "user-cloud-policy-manager-connect-trace");
    crash_reporter::SetCrashKeyStringToStackTrace(&connect_callstack_key,
                                                  connect_callstack_);
  } else {
    connect_callstack_ = base::debug::StackTrace();
  }
  CHECK(!core()->client());

  local_state_ = local_state;
  // Note: |system_url_loader_factory| can be null for tests.
  // Use the system URL loader context here instead of a context derived
  // from the Profile because Connect() is called before the profile is
  // fully initialized (required so we can perform the initial policy load).
  std::unique_ptr<CloudPolicyClient> cloud_policy_client =
      std::make_unique<CloudPolicyClient>(
          std::string() /* machine_id */, std::string() /* machine_model */,
          std::string() /* brand_code */, device_management_service,
          system_url_loader_factory, nullptr /* signing_service */,
          chromeos::GetDeviceDMTokenForUserPolicyGetter(account_id_));
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
    CHECK(!waiting_for_policy_fetch_ ||
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              chromeos::switches::kWaitForInitialPolicyFetchForTest));
    if (!client()->is_registered() &&
        enforcement_type_ != PolicyEnforcement::kPolicyOptional) {
      // We expected to load policy, but we don't have policy, so exit the
      // session.
      LOG(ERROR) << "Failed to load policy during synchronous restart "
                 << "- terminating session";
      if (fatal_error_callback_)
        std::move(fatal_error_callback_).Run();
      return;
    }

    // Initialization has completed before our observer was registered
    // so invoke our callback directly.
    OnInitializationCompleted(service());
  } else {
    // Wait for the CloudPolicyStore to finish initializing.
    service()->AddObserver(this);
  }

  app_install_event_log_uploader_ =
      std::make_unique<AppInstallEventLogUploader>(client());
}

void UserCloudPolicyManagerChromeOS::OnAccessTokenAvailable(
    const std::string& access_token) {
  access_token_ = access_token;

  if (!wildcard_username_.empty()) {
    wildcard_login_checker_.reset(new WildcardLoginChecker());
    // Safe to set a callback with an unretained pointer because the
    // WildcardLoginChecker is owned by this object and won't invoke the
    // callback after we destroy it.
    wildcard_login_checker_->StartWithAccessToken(
        access_token,
        base::BindOnce(
            &UserCloudPolicyManagerChromeOS::OnWildcardCheckCompleted,
            base::Unretained(this), wildcard_username_));
  }

  if (service() && service()->IsInitializationComplete() &&
      client() && !client()->is_registered()) {
    OnOAuth2PolicyTokenFetched(
        access_token, GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }
}

void UserCloudPolicyManagerChromeOS::OnWildcardCheckCompleted(
    const std::string& username,
    WildcardLoginChecker::Result result) {
  if (result == WildcardLoginChecker::RESULT_BLOCKED) {
    LOG(ERROR) << "Online wildcard login check failed, terminating session.";

    // TODO(mnissler): This only removes the user pod from the login screen, but
    // the cryptohome remains. This is because deleting the cryptohome for a
    // logged-in session is not possible. Fix this either by delaying the
    // cryptohome deletion operation or by getting rid of the in-session
    // wildcard check.
    user_manager::UserManager::Get()->RemoveUserFromList(
        AccountId::FromUserEmail(username));
    if (fatal_error_callback_)
      std::move(fatal_error_callback_).Run();
  }
}

bool UserCloudPolicyManagerChromeOS::IsClientRegistered() const {
  return client() && client()->is_registered();
}

void UserCloudPolicyManagerChromeOS::EnableWildcardLoginCheck(
    const std::string& username) {
  DCHECK(access_token_.empty());
  wildcard_username_ = username;
}

AppInstallEventLogUploader*
UserCloudPolicyManagerChromeOS::GetAppInstallEventLogUploader() {
  return app_install_event_log_uploader_.get();
}

void UserCloudPolicyManagerChromeOS::Shutdown() {
  app_install_event_log_uploader_.reset();
  if (client())
    client()->RemoveObserver(this);
  if (service())
    service()->RemoveObserver(this);
  token_fetcher_.reset();
  external_data_manager_->Disconnect();
  CloudPolicyManager::Shutdown();
}

bool UserCloudPolicyManagerChromeOS::IsInitializationComplete(
    PolicyDomain domain) const {
  if (!CloudPolicyManager::IsInitializationComplete(domain))
    return false;
  if (domain == POLICY_DOMAIN_CHROME)
    return !waiting_for_policy_fetch_;
  return true;
}

void UserCloudPolicyManagerChromeOS::OnInitializationCompleted(
    CloudPolicyService* cloud_policy_service) {
  DCHECK_EQ(service(), cloud_policy_service);
  cloud_policy_service->RemoveObserver(this);

  time_init_completed_ = base::Time::Now();
  UMA_HISTOGRAM_MEDIUM_TIMES(kUMADelayInitialization,
                             time_init_completed_ - time_init_started_);

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

  if (!waiting_for_policy_fetch_) {
    // If this isn't blocking on a policy fetch then
    // CloudPolicyManager::OnStoreLoaded() already published the cached policy.
    // Start the refresh scheduler now, which will eventually refresh the
    // cached policy or make the first fetch once the OAuth2 token is
    // available.
    StartRefreshSchedulerIfReady();
  }
}

void UserCloudPolicyManagerChromeOS::OnPolicyFetched(
    CloudPolicyClient* client) {
  // No action required. If we're blocked on a policy fetch, we'll learn about
  // completion of it through OnInitialPolicyFetchComplete().
}

void UserCloudPolicyManagerChromeOS::OnRegistrationStateChanged(
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
    time_client_registered_ = base::Time::Now();
    if (!time_token_available_.is_null()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          kUMAInitialFetchDelayClientRegister,
          time_client_registered_ - time_token_available_);
    }

    // If we're blocked on the policy fetch, now is a good time to issue it.
    if (client()->is_registered()) {
      service()->RefreshPolicy(
          base::Bind(
              &UserCloudPolicyManagerChromeOS::OnInitialPolicyFetchComplete,
              base::Unretained(this)));
    } else {
      // If the client has switched to not registered, we bail out as this
      // indicates the cloud policy setup flow has been aborted.
      CancelWaitForPolicyFetch(true);
    }
  }
}

void UserCloudPolicyManagerChromeOS::OnClientError(
    CloudPolicyClient* cloud_policy_client) {
  DCHECK_EQ(client(), cloud_policy_client);
  if (waiting_for_policy_fetch_) {
    base::UmaHistogramSparse(kUMAInitialFetchClientError,
                             cloud_policy_client->status());
  }
  switch (client()->status()) {
    case DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
      // If management is not supported for this user, then a registration
      // error is to be expected - treat as a policy fetch success. Also
      // mark this profile as not requiring policy.
      SetPolicyRequired(false);
      CancelWaitForPolicyFetch(true);
      break;
    case DM_STATUS_SUCCESS:
      CancelWaitForPolicyFetch(true);
      break;
    default:
      // Unexpected error fetching policy.
      CancelWaitForPolicyFetch(false);
      break;
  }
  // If we are in re-registration state and re-registration fails, we mark the
  // user to require an online sign-in on his next sign-in.
  if (is_in_reregistration_state_) {
    RegistrationResultUMA(RegistrationResult::kReregistrationUnsuccessful);
    LOG(ERROR) << "Re-registration failed, requiring the user to perform an "
                  "online sign-in.";
    chromeos::ChromeUserManager::Get()->SaveForceOnlineSignin(account_id_,
                                                              true);
  }
}

void UserCloudPolicyManagerChromeOS::OnComponentCloudPolicyUpdated() {
  CloudPolicyManager::OnComponentCloudPolicyUpdated();
  StartRefreshSchedulerIfReady();
}

void UserCloudPolicyManagerChromeOS::OnStoreLoaded(
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
    chromeos::AffiliationIDSet set_of_user_affiliation_ids(
        policy_data->user_affiliation_ids().begin(),
        policy_data->user_affiliation_ids().end());

    chromeos::ChromeUserManager::Get()->SetUserAffiliation(
        account_id_, set_of_user_affiliation_ids);
  }
}

void UserCloudPolicyManagerChromeOS::SetPolicyRequired(bool policy_required) {
  chromeos::ChromeUserManager* user_manager =
      chromeos::ChromeUserManager::Get();
  user_manager::known_user::SetProfileRequiresPolicy(
      account_id_,
      policy_required
          ? user_manager::known_user::ProfileRequiresPolicy::kPolicyRequired
          : user_manager::known_user::ProfileRequiresPolicy::kNoPolicyRequired);
  if (user_manager->IsCurrentUserNonCryptohomeDataEphemeral()) {
    // For ephemeral users, we need to set a flag via session manager - this
    // handles the case where the session restarts due to a crash (the restarted
    // instance will know whether policy is required via this flag). This
    // overwrites flags set by about://flags, but that's OK since we can't have
    // any of those flags set at startup anyway for ephemeral sessions.
    base::CommandLine command_line =
        base::CommandLine(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(chromeos::switches::kProfileRequiresPolicy,
                                   policy_required ? "true" : "false");
    base::CommandLine::StringVector flags;
    flags.assign(command_line.argv().begin() + 1, command_line.argv().end());
    DCHECK_EQ(1u, flags.size());
    chromeos::UserSessionManager::GetInstance()->SetSwitchesForUser(
        account_id_,
        chromeos::UserSessionManager::CommandLineSwitchesType::kSessionControl,
        flags);
  }
}

void UserCloudPolicyManagerChromeOS::GetChromePolicy(PolicyMap* policy_map) {
  CloudPolicyManager::GetChromePolicy(policy_map);

  // If the store has a verified policy blob received from the server then apply
  // the defaults for policies that haven't been configured by the administrator
  // given that this is an enterprise user.
  if (!store()->has_policy())
    return;

  // Don't apply enterprise defaults for Child user.
  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id_);
  if (user && user->GetType() == user_manager::USER_TYPE_CHILD)
    return;

  SetEnterpriseUsersDefaults(policy_map);
}

void UserCloudPolicyManagerChromeOS::FetchPolicyOAuthToken() {
  // By-pass token fetching for test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableGaiaServices)) {
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
      chromeos::UserSessionManager::GetInstance()
          ->user_context()
          .GetRefreshToken());

  if (!refresh_token.empty()) {
    token_fetcher_.reset(PolicyOAuth2TokenFetcher::CreateInstance());
    token_fetcher_->StartWithRefreshToken(
        refresh_token, system_url_loader_factory,
        base::Bind(&UserCloudPolicyManagerChromeOS::OnOAuth2PolicyTokenFetched,
                   base::Unretained(this)));
    return;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kWaitForInitialPolicyFetchForTest)) {
    // Some tests don't want to complete policy initialization until they have
    // manually injected policy. Do not treat this as a policy fetch error.
    return;
  }

  LOG(ERROR) << "No refresh token for policy oauth token fetch!";
  OnOAuth2PolicyTokenFetched(
      std::string(),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

void UserCloudPolicyManagerChromeOS::OnOAuth2PolicyTokenFetched(
    const std::string& policy_token,
    const GoogleServiceAuthError& error) {
  DCHECK(!client()->is_registered());
  time_token_available_ = base::Time::Now();
  if (waiting_for_policy_fetch_) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMAInitialFetchDelayOAuth2Token,
                               time_token_available_ - time_init_completed_);
  }

  if (error.state() == GoogleServiceAuthError::NONE) {
    // Start client registration. Either OnRegistrationStateChanged() or
    // OnClientError() will be called back.
    const auto lifetime =
        user_manager::UserManager::Get()->IsCurrentUserCryptohomeDataEphemeral()
            ? em::DeviceRegisterRequest::LIFETIME_EPHEMERAL_USER
            : em::DeviceRegisterRequest::LIFETIME_INDEFINITE;
    std::string client_id;
    if (client()->requires_reregistration())
      client_id = client()->client_id();
    client()->Register(em::DeviceRegisterRequest::USER,
                       em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION,
                       lifetime, em::LicenseType::UNDEFINED,
                       DMAuth::FromOAuthToken(policy_token), client_id,
                       std::string(), std::string());
  } else {
    UMA_HISTOGRAM_ENUMERATION(kUMAInitialFetchOAuth2Error, error.state(),
                              GoogleServiceAuthError::NUM_STATES);
    if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED) {
      // Network errors are negative in the code, but the histogram data type
      // expects the corresponding positive value.
      base::UmaHistogramSparse(kUMAInitialFetchOAuth2NetworkError,
                               -error.network_error());
    }
    // Failed to get a token, stop waiting if policy is not required for this
    // user.
    CancelWaitForPolicyFetch(false);
  }

  token_fetcher_.reset();
}

void UserCloudPolicyManagerChromeOS::OnInitialPolicyFetchComplete(
    bool success) {
  const base::Time now = base::Time::Now();
  UMA_HISTOGRAM_MEDIUM_TIMES(kUMAInitialFetchDelayPolicyFetch,
                             now - time_client_registered_);
  UMA_HISTOGRAM_MEDIUM_TIMES(kUMAInitialFetchDelayTotal,
                             now - time_init_started_);
  CancelWaitForPolicyFetch(success);
}

void UserCloudPolicyManagerChromeOS::OnPolicyRefreshTimeout() {
  DCHECK(waiting_for_policy_fetch_);
  LOG(WARNING) << "Timed out while waiting for the policy refresh. "
               << "The session will start with the cached policy.";
  CancelWaitForPolicyFetch(false);
}

void UserCloudPolicyManagerChromeOS::CancelWaitForPolicyFetch(bool success) {
  if (!waiting_for_policy_fetch_)
    return;

  policy_refresh_timeout_.Stop();

  // If there was an error, and we don't want to allow profile initialization
  // to go forward after a failed policy fetch, then trigger a fatal error.
  if (!success && enforcement_type_ != PolicyEnforcement::kPolicyOptional) {
    LOG(ERROR) << "Policy fetch failed for the user. "
                  "Aborting profile initialization";
    // Need to exit the current user, because we've already started this user's
    // session.
    if (fatal_error_callback_)
      std::move(fatal_error_callback_).Run();
    return;
  }

  waiting_for_policy_fetch_ = false;

  CheckAndPublishPolicy();
  // Now that |waiting_for_policy_fetch_| is guaranteed to be false, the
  // scheduler can be started.
  StartRefreshSchedulerIfReady();
}

void UserCloudPolicyManagerChromeOS::StartRefreshSchedulerIfReady() {
  if (core()->refresh_scheduler())
    return;  // Already started.

  if (waiting_for_policy_fetch_)
    return;  // Still waiting for the initial, blocking fetch.

  if (!service() || !local_state_)
    return;  // Not connected.

  if (component_policy_service() &&
      !component_policy_service()->is_initialized()) {
    // If the client doesn't have the list of components to fetch yet then don't
    // start the scheduler. The |component_policy_service_| will call back into
    // OnComponentCloudPolicyUpdated() once it's ready.
    return;
  }

  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state_,
                                policy_prefs::kUserPolicyRefreshRate);
}

void UserCloudPolicyManagerChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_ADDED, type);

  // Now that the profile is fully created we can unsubscribe from the
  // notification.
  registrar_.Remove(this, chrome::NOTIFICATION_PROFILE_ADDED,
                    content::Source<Profile>(profile_));

  invalidation::ProfileInvalidationProvider* const invalidation_provider =
      invalidation::DeprecatedProfileInvalidationProviderFactory::GetForProfile(
          profile_);

  if (!invalidation_provider)
    return;

  core()->StartRemoteCommandsService(
      std::make_unique<UserCommandsFactoryChromeOS>(profile_));
  invalidator_ = std::make_unique<RemoteCommandsInvalidatorImpl>(core());
  invalidator_->Initialize(invalidation_provider->GetInvalidationService());

  shutdown_notifier_ =
      UserCloudPolicyManagerChromeOSNotifierFactory::GetInstance()
          ->Get(profile_)
          ->Subscribe(base::AdaptCallbackForRepeating(
              base::BindOnce(&UserCloudPolicyManagerChromeOS::ProfileShutdown,
                             base::Unretained(this))));
}

void UserCloudPolicyManagerChromeOS::ProfileShutdown() {
  // Unregister the RemoteCommandsInvalidatorImpl from the InvalidatorRegistrar.
  invalidator_->Shutdown();
  invalidator_.reset();
  shutdown_notifier_.reset();
}

void UserCloudPolicyManagerChromeOS::SetUserContextRefreshTokenForTests(
    const std::string& refresh_token) {
  DCHECK(!refresh_token.empty());
  DCHECK(!user_context_refresh_token_for_tests_);
  user_context_refresh_token_for_tests_ = base::make_optional(refresh_token);
}

}  // namespace policy
