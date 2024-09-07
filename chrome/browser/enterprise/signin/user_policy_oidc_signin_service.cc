// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor.h"
#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor_factory.h"
#include "chrome/browser/enterprise/signin/oidc_metrics_utils.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_internal.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace {

bool IsOidcManagedProfile(Profile* profile) {
  auto* entry = g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile->GetPath());

  return entry && !entry->GetProfileManagementOidcTokens().auth_token.empty() &&
         !entry->GetProfileManagementOidcTokens().id_token.empty();
}

bool IsDasherlessProfile(Profile* profile) {
  auto* entry = g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile->GetPath());

  return IsOidcManagedProfile(profile) && entry->IsDasherlessManagement();
}

}  // namespace

namespace policy {

// Take over policy management from UserPolicySigninService to avoid conflicts
void ShutdownUserPolicySigninService(Profile* profile) {
  // Shutdown `UserPolicySigninService` if needed for Dasher-based profiles, so
  // this class can be the only sign in service controlling
  // UserCloudPolicyManager.
  if (!IsDasherlessProfile(profile)) {
    auto* user_policy_signin_service =
        UserPolicySigninServiceFactory::GetForProfile(profile);
    // Only one sign in service should be managing policies, so we need to
    // shutdown `UserPolicySigninService` before starting restoration process.
    if (user_policy_signin_service) {
      VLOG_POLICY(2, OIDC_ENROLLMENT) << "Shutting down user policy signin "
                                         "service in favor of OIDC service";
      user_policy_signin_service->Shutdown();
    }
  }
}

OidcProfileManagerObserverBridge::OidcProfileManagerObserverBridge(
    UserPolicyOidcSigninService* user_policy_signin_service)
    : user_policy_signin_service_(user_policy_signin_service) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    profile_manager_observation_.Observe(profile_manager);
  }
}

void OidcProfileManagerObserverBridge::OnProfileAdded(Profile* profile) {
  user_policy_signin_service_->OnProfileReady(profile);
}

void OidcProfileManagerObserverBridge::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

OidcProfileManagerObserverBridge::~OidcProfileManagerObserverBridge() = default;

UserPolicyOidcSigninService::UserPolicyOidcSigninService(
    Profile* profile,
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    absl::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
        policy_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory)
    : UserPolicySigninServiceBase(local_state,
                                  device_management_service,
                                  policy_manager,
                                  identity_manager,
                                  system_url_loader_factory),
      profile_(profile) {}

UserPolicyOidcSigninService::~UserPolicyOidcSigninService() = default;

void UserPolicyOidcSigninService::OnProfileReady(Profile* profile) {
  if (profile && profile == profile_ && IsOidcManagedProfile(profile)) {
    InitializeOnProfileReady(profile);
  }
}

void UserPolicyOidcSigninService::
    OnCloudPolicyServiceInitializationCompleted() {
  CloudPolicyManager* manager = policy_manager();
  CHECK(manager->core()->service()->IsInitializationComplete());
  // The service is now initialized - if the client is not yet registered, then
  // it means that there is no cached policy and so we need to initiate a new
  // client registration.
  if (manager->IsClientRegistered()) {
    return;
  }

  // TODO(326078013): Policy client is current incapable of re-regstration due
  // to the quick expiration of OIDC responses. Add client registration here
  // when it's implemented.
}

void UserPolicyOidcSigninService::OnPolicyFetched(CloudPolicyClient* client) {}

void UserPolicyOidcSigninService::FetchPolicyForOidcUser(
    const AccountId& account_id,
    const std::string& dm_token,
    const std::string& client_id,
    const std::string& user_email,
    const std::vector<std::string>& user_affiliation_ids,
    base::TimeTicks policy_fetch_start_time,
    bool switch_to_entry,
    scoped_refptr<network::SharedURLLoaderFactory> profile_url_loader_factory,
    PolicyFetchCallback callback) {
  ShutdownUserPolicySigninService(profile_);
  FetchPolicyForSignedInUser(
      account_id, dm_token, client_id, user_affiliation_ids,
      std::move(profile_url_loader_factory),
      base::BindOnce(&policy::UserPolicyOidcSigninService::
                         OnPolicyFetchCompleteInNewProfile,
                     weak_factory_.GetWeakPtr(), user_email,
                     policy_fetch_start_time, switch_to_entry,
                     std::move(callback)));
}

void UserPolicyOidcSigninService::AttemptToRestorePolicy() {
  ShutdownUserPolicySigninService(profile_);
  VLOG_POLICY(2, OIDC_ENROLLMENT)
      << "Attempting to restore OIDC profile policy via backup DM token";
  std::string dm_token = profile_->GetPrefs()->GetString(
      enterprise_signin::prefs::kPolicyRecoveryToken);
  if (dm_token.empty()) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "OIDC policy restoration failed due to missing back up DM token.";
    return;
  }

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());

  if (!entry) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "OIDC policy restoration failed due to missing profile attribute.";
    return;
  }

  // Policy restoration only applies to OIDC profiles and when there is no other
  // restoration/interception in progress.
  if (entry->GetProfileManagementOidcTokens().auth_token.empty()) {
    LOG_POLICY(ERROR, OIDC_ENROLLMENT)
        << "Policy restoration is only available for OIDC-managed profiles.";
    return;
  }

  std::string client_id = profile_->GetPrefs()->GetString(
      enterprise_signin::prefs::kPolicyRecoveryClientId);

  VLOG_POLICY(2, OIDC_ENROLLMENT)
      << "Starting OIDC policy recovery using client ID: " << client_id;

  FetchPolicyForOidcUser(AccountId(), dm_token, client_id,
                         profile_->GetPrefs()->GetString(
                             enterprise_signin::prefs::kProfileUserEmail),
                         /*user_affiliation_ids=*/std::vector<std::string>(),
                         base::TimeTicks::Now(), /*switch_to_entry=*/false,
                         profile_->GetDefaultStoragePartition()
                             ->GetURLLoaderFactoryForBrowserProcess(),
                         base::BindOnce([](bool) {}));
}

void UserPolicyOidcSigninService::OnStoreLoaded(CloudPolicyStore* store) {
  store->RemoveObserver(this);
}

void UserPolicyOidcSigninService::OnStoreError(CloudPolicyStore* store) {
  store->RemoveObserver(this);
  AttemptToRestorePolicy();
}

void UserPolicyOidcSigninService::OnPolicyFetchCompleteInNewProfile(
    std::string user_email,
    base::TimeTicks policy_fetch_start_time,
    bool switch_to_entry,
    PolicyFetchCallback callback,
    bool success) {
  bool dasher_based = !IsDasherlessProfile(profile_);
  RecordOidcEnrollmentPolicyFetchLatency(
      dasher_based, success, base::TimeTicks::Now() - policy_fetch_start_time);
  if (success) {
    VLOG_POLICY(2, OIDC_ENROLLMENT) << "Policy fetched for OIDC profile.";
    profile_->GetPrefs()->SetBoolean(
        enterprise_signin::prefs::kPolicyRecoveryRequired, false);
  } else {
    profile_->GetPrefs()->SetBoolean(
        enterprise_signin::prefs::kPolicyRecoveryRequired, true);
  }

  if (success && dasher_based && !switch_to_entry) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);

    // Account already exists, no need to add again.
    if (!identity_manager->FindExtendedAccountInfoByEmailAddress(user_email)
             .IsEmpty()) {
      return;
    }

    VLOG_POLICY(2, OIDC_ENROLLMENT)
        << "Policy fetched for Dasher-based OIDC profile, adding the user as "
           "the primary account.";
    RecordOidcProfileCreationFunnelStep(
        OidcProfileCreationFunnelStep::kAddingPrimaryAccount, dasher_based);

    // User account management would be included in unified consent dialog.
    enterprise_util::SetUserAcceptedAccountManagement(profile_, true);
    CHECK(profile_);
    policy::CloudPolicyManager* user_policy_manager =
        profile_->GetUserCloudPolicyManager();

    std::string gaia_id =
        user_policy_manager->core()->store()->policy()->gaia_id();

    VLOG_POLICY(2, OIDC_ENROLLMENT) << "GAIA ID retrieved from user policy for "
                                    << user_email << ": " << gaia_id << ".";

    auto set_primary_account_result =
        signin_util::SetPrimaryAccountWithInvalidToken(
            profile_, user_email, gaia_id,
            /*is_under_advanced_protection=*/false,
            signin_metrics::AccessPoint::
                ACCESS_POINT_OIDC_REDIRECTION_INTERCEPTION,
            signin_metrics::SourceForRefreshTokenOperation::
                kMachineLogon_CredentialProvider);

    VLOG_POLICY(2, OIDC_ENROLLMENT)
        << "Operation of setting account id " << gaia_id
        << " received the following result: "
        << static_cast<int>(set_primary_account_result);

    RecordOidcProfileCreationResult(
        (set_primary_account_result ==
         signin::PrimaryAccountMutator::PrimaryAccountError::kNoError)
            ? OidcProfileCreationResult::kEnrollmentSucceeded
            : OidcProfileCreationResult::kFailedToAddPrimaryAccount,
        dasher_based);

  } else {
    RecordOidcProfileCreationResult(
        (success) ? ((switch_to_entry)
                         ? OidcProfileCreationResult::kSwitchedToExistingProfile
                         : OidcProfileCreationResult::kEnrollmentSucceeded)
                  : OidcProfileCreationResult::kFailedToFetchPolicy,
        dasher_based);
  }

  std::move(callback).Run(success);
}

void UserPolicyOidcSigninService::InitializeCloudPolicyManager(
    const AccountId& account_id,
    std::unique_ptr<CloudPolicyClient> client) {
  if (!IsDasherlessProfile(profile_)) {
    UserCloudPolicyManager* manager =
        static_cast<UserCloudPolicyManager*>(policy_manager());
    manager->SetSigninAccountId(account_id);
  } else {
    CHECK(!account_id.is_valid());
  }
  UserPolicySigninServiceBase::InitializeCloudPolicyManager(account_id,
                                                            std::move(client));
}

std::string UserPolicyOidcSigninService::GetProfileId() {
  return ::policy::GetProfileId(profile_);
}

bool UserPolicyOidcSigninService::CanApplyPolicies(
    bool check_for_refresh_token) {
  return g_browser_process->profile_manager() && IsOidcManagedProfile(profile_);
}

void UserPolicyOidcSigninService::InitializeOnProfileReady(Profile* profile) {
  DCHECK_EQ(profile, profile_);
  VLOG_POLICY(2, OIDC_ENROLLMENT)
      << "Initializing OIDC Signin Service for profile " << GetProfileId();
  // If using a TestingProfile with no IdentityManager or
  // CloudPolicyManager, skip initialization.
  if (!policy_manager() || !identity_manager()) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  // UserPolicyOidcSigninService is only responsible for OIDC-managed profiles.
  if (!IsOidcManagedProfile(profile)) {
    return;
  }

  auto* policy_store = policy_manager()->core()->store();
  if (!policy_store->is_initialized()) {
    store_observation_.Observe(policy_store);
  } else if (policy_manager()->core()->store()->status() !=
             CloudPolicyStore::Status::STATUS_OK) {
    VLOG_POLICY(2, OIDC_ENROLLMENT) << "Cached OIDC policies are not valid";

    AttemptToRestorePolicy();

    // If policy already exists and profile is dasher-based, initialization will
    // be taken care of by `UserPolicySigninService`. If there's no policy yet,
    // then the first policy fetch is still in progress, and initialization will
    // be done via `FetchPolicyForSignedInUser`.
  } else if (!policy_manager()->core()->store()->is_managed() &&
             profile_->GetPrefs()->GetBoolean(
                 enterprise_signin::prefs::kPolicyRecoveryRequired)) {
    VLOG_POLICY(2, OIDC_ENROLLMENT)
        << "OIDC policy is missing due to a previous fetch failure. ";

    AttemptToRestorePolicy();

  } else if (policy_manager()->core()->store()->is_managed() &&
             IsDasherlessProfile(profile)) {
    VLOG_POLICY(2, OIDC_ENROLLMENT)
        << "OIDC Signin Service Initializing for Dasherless Profile";
    InitializeForSignedInUser(AccountId(),
                              profile->GetDefaultStoragePartition()
                                  ->GetURLLoaderFactoryForBrowserProcess());
  }
}

std::string_view UserPolicyOidcSigninService::name() const {
  return "UserPolicyOidcSigninService";
}

}  // namespace policy
