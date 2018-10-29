// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/policy/wildcard_login_checker.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GoogleServiceAuthError;
class PrefService;
class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class AppInstallEventLogUploader;
class CloudExternalDataManager;
class DeviceManagementService;
class PolicyOAuth2TokenFetcher;
class RemoteCommandsInvalidator;

// Implements logic for initializing user policy on Chrome OS.
class UserCloudPolicyManagerChromeOS : public CloudPolicyManager,
                                       public CloudPolicyClient::Observer,
                                       public CloudPolicyService::Observer,
                                       public content::NotificationObserver,
                                       public KeyedService {
 public:
  // Enum describing what behavior we want to enforce here.
  enum class PolicyEnforcement {
    // No policy enforcement - it's OK to start the session even without a
    // policy check because it has previously been established that this
    // session is unmanaged.
    kPolicyOptional,

    // This is a managed session so require a successful policy load before
    // completing profile initialization.
    kPolicyRequired,

    // It is unknown whether this session should be managed, so require a check
    // with the policy server before initializing the profile.
    kServerCheckRequired
  };

  // |enforcement_type| specifies what kind of policy state will be
  // enforced by this object:
  //
  // * kPolicyOptional: The class will kick off a background policy fetch to
  //   detect whether the user has become managed since the last signin, but
  //   there's no enforcement (it's OK for the server request to fail, and the
  //   profile initialization is allowed to proceed immediately).
  //
  // * kServerCheckRequired: Profile initialization will be blocked
  //   (IsInitializationComplete() will return false) until we have made a
  //   successful call to DMServer to check for policy or loaded policy from
  //   cache. If this call is unsuccessful due to network/server errors, then
  //   |fatal_error_callback| is invoked to close the session.
  //
  // * kPolicyRequired: A background policy refresh will be initiated. If a
  //   non-zero |policy_refresh_timeout| is passed, then profile initialization
  //   will be blocked (IsInitializationComplete() will return false) until
  //   either the fetch completes or the timeout fires. |fatal_error_callback|
  //   will be invoked if the system could not load policy from either cache or
  //   the server.
  //
  // |account_id| is the AccountId associated with the user's session.
  // |task_runner| is the runner for policy refresh tasks.
  UserCloudPolicyManagerChromeOS(
      Profile* profile,
      std::unique_ptr<CloudPolicyStore> store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager,
      const base::FilePath& component_policy_cache_path,
      PolicyEnforcement enforcement_type,
      base::TimeDelta policy_refresh_timeout,
      base::OnceClosure fatal_error_callback,
      const AccountId& account_id,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~UserCloudPolicyManagerChromeOS() override;

  // Initializes the cloud connection. |local_state| and
  // |device_management_service| must stay valid until this object is deleted.
  void Connect(
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);

  // This class is one of the policy providers, and must be ready for the
  // creation of the Profile's PrefService; all the other KeyedServices depend
  // on the PrefService, so this class can't depend on other
  // BrowserContextKeyedServices to avoid a circular dependency. So instead of
  // using the ProfileOAuth2TokenService directly to get the access token, a 3rd
  // service (UserCloudPolicyTokenForwarder) will fetch it later and pass it to
  // this method once available.
  // The |access_token| can then be used to authenticate the registration
  // request to the DMServer.
  void OnAccessTokenAvailable(const std::string& access_token);

  // Returns true if the underlying CloudPolicyClient is already registered.
  bool IsClientRegistered() const;

  // Indicates a wildcard login check should be performed once an access token
  // is available.
  void EnableWildcardLoginCheck(const std::string& username);

  // Return the AppInstallEventLogUploader used to send app push-install event
  // logs to the policy server.
  AppInstallEventLogUploader* GetAppInstallEventLogUploader();

  // ConfigurationPolicyProvider:
  void Shutdown() override;
  bool IsInitializationComplete(PolicyDomain domain) const override;

  // CloudPolicyService::Observer:
  void OnInitializationCompleted(CloudPolicyService* service) override;

  // CloudPolicyClient::Observer:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // ComponentCloudPolicyService::Delegate:
  void OnComponentCloudPolicyUpdated() override;

  // CloudPolicyManager:
  void OnStoreLoaded(CloudPolicyStore* cloud_policy_store) override;

  // Helper function to force a policy fetch timeout.
  void ForceTimeoutForTest();

  // Sets the SharedURLLoaderFactory's that should be used for tests instead of
  // retrieving one from the BrowserProcess object in FetchPolicyOAuthToken().
  void SetSignInURLLoaderFactoryForTests(
      scoped_refptr<network::SharedURLLoaderFactory> signin_url_loader_factory);
  void SetSystemURLLoaderFactoryForTests(
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);

  // Set a refresh token to be used in tests instead of the user context refresh
  // token when fetching the policy OAuth token.
  void SetUserContextRefreshTokenForTests(const std::string& refresh_token);

 protected:
  // CloudPolicyManager:
  void GetChromePolicy(PolicyMap* policy_map) override;

 private:
  // Sets the appropriate persistent flags to mark whether the current session
  // requires policy. If |policy_required| is true, this ensures that future
  // instances of this session will not start up unless a valid policy blob can
  // be loaded.
  void SetPolicyRequired(bool policy_required);

  // Fetches a policy token using the refresh token if available, or the
  // authentication context of the signin context, and calls back
  // OnOAuth2PolicyTokenFetched when done.
  void FetchPolicyOAuthToken();

  // Called once the policy access token is available, and starts the
  // registration with the policy server if the token was successfully fetched.
  void OnOAuth2PolicyTokenFetched(const std::string& policy_token,
                                  const GoogleServiceAuthError& error);

  // Completion handler for the explicit policy fetch triggered on startup in
  // case |waiting_for_policy_fetch_| is true. |success| is true if the
  // fetch was successful.
  void OnInitialPolicyFetchComplete(bool success);

  // Called when |policy_refresh_timeout_| times out, to cancel the blocking
  // wait for the policy refresh.
  void OnPolicyRefreshTimeout();

  // Called when a wildcard check has completed, to allow us to exit the session
  // if required.
  void OnWildcardCheckCompleted(const std::string& username,
                                WildcardLoginChecker::Result result);

  // Cancels waiting for the initial policy fetch/refresh and flags the
  // ConfigurationPolicyProvider ready (assuming all other initialization tasks
  // have completed). Pass |true| if policy fetch was successful (either because
  // policy was successfully fetched, or if DMServer has notified us that the
  // user is not managed).
  void CancelWaitForPolicyFetch(bool success);

  void StartRefreshSchedulerIfReady();

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Observer called on profile shutdown.
  void ProfileShutdown();

  // Profile associated with the current user.
  Profile* const profile_;

  // Owns the store, note that CloudPolicyManager just keeps a plain pointer.
  std::unique_ptr<CloudPolicyStore> store_;

  // Manages external data referenced by policies.
  std::unique_ptr<CloudExternalDataManager> external_data_manager_;

  // Helper used to send app push-install event logs to the policy server.
  std::unique_ptr<AppInstallEventLogUploader> app_install_event_log_uploader_;

  // Username for the wildcard login check if applicable, empty otherwise.
  std::string wildcard_username_;

  // Path where policy for components will be cached.
  base::FilePath component_policy_cache_path_;

  // Whether we're waiting for a policy fetch to complete before reporting
  // IsInitializationComplete().
  bool waiting_for_policy_fetch_;

  // What kind of enforcement we need to implement.
  PolicyEnforcement enforcement_type_;

  // A timer that puts a hard limit on the maximum time to wait for a policy
  // refresh.
  base::OneShotTimer policy_refresh_timeout_;

  // The pref service to pass to the refresh scheduler on initialization.
  PrefService* local_state_;

  // Used to fetch the policy OAuth token, when necessary. This object holds
  // a callback with an unretained reference to the manager, when it exists.
  std::unique_ptr<PolicyOAuth2TokenFetcher> token_fetcher_;

  // Keeps alive the wildcard checker while its running.
  std::unique_ptr<WildcardLoginChecker> wildcard_login_checker_;

  // The access token passed to OnAccessTokenAvailable. It is stored here so
  // that it can be used if OnInitializationCompleted is called later.
  std::string access_token_;

  // Timestamps for collecting timing UMA stats.
  base::Time time_init_started_;
  base::Time time_init_completed_;
  base::Time time_token_available_;
  base::Time time_client_registered_;

  // Stack trace of the previous Connect() method call.
  // TODO(emaxx): Remove after the crashes tracked at https://crbug.com/685996
  // are fixed.
  base::debug::StackTrace connect_callstack_;

  // The AccountId associated with the user whose policy is being loaded.
  AccountId account_id_;

  // The callback to invoke if the user session should be shutdown. This is
  // injected in the constructor to make it easier to write tests.
  base::OnceClosure fatal_error_callback_;

  // Used to register for notification that profile creation is complete.
  content::NotificationRegistrar registrar_;

  // Invalidator used for remote commands to be delivered to this user.
  std::unique_ptr<RemoteCommandsInvalidator> invalidator_;

  // Listening to notification that profile is destroyed.
  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      shutdown_notifier_;

  // The SharedURLLoaderFactory used in some tests to simulate network requests.
  scoped_refptr<network::SharedURLLoaderFactory>
      system_url_loader_factory_for_tests_;
  scoped_refptr<network::SharedURLLoaderFactory>
      signin_url_loader_factory_for_tests_;

  // Refresh token used in tests instead of the user context refresh token to
  // fetch the policy OAuth token.
  base::Optional<std::string> user_context_refresh_token_for_tests_;

  // Used to track the reregistration state of the CloudPolicyClient, i.e.
  // whether this class has triggered a re-registration after the client failed
  // to load policy with error |DM_STATUS_SERVICE_DEVICE_NOT_FOUND|.
  bool is_in_reregistration_state_ = false;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_CHROMEOS_H_
