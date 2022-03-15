// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {

namespace {

#if BUILDFLAG(IS_ANDROID)
const em::DeviceRegisterRequest::Type kCloudPolicyRegistrationType =
    em::DeviceRegisterRequest::ANDROID_BROWSER;
#else
#error "This file can be built only on OS_ANDROID."
#endif

}  // namespace

UserPolicySigninService::UserPolicySigninService(
    Profile* profile,
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    UserCloudPolicyManager* policy_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory)
    : UserPolicySigninServiceBase(local_state,
                                  device_management_service,
                                  policy_manager,
                                  identity_manager,
                                  system_url_loader_factory),
      profile_prefs_(profile->GetPrefs()),
      profile_(profile) {
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->AddObserver(this);
}

UserPolicySigninService::~UserPolicySigninService() {}

void UserPolicySigninService::ShutdownUserCloudPolicyManager() {
  CancelPendingRegistration();
  UserPolicySigninServiceBase::ShutdownUserCloudPolicyManager();
}

void UserPolicySigninService::RegisterForPolicyWithAccountId(
    const std::string& username,
    const CoreAccountId& account_id,
    PolicyRegistrationCallback callback) {
  // Create a new CloudPolicyClient for fetching the DMToken.
  std::unique_ptr<CloudPolicyClient> policy_client =
      CreateClientForRegistrationOnly(username);
  if (!policy_client) {
    std::move(callback).Run(std::string(), std::string());
    return;
  }

  CancelPendingRegistration();

  // Fire off the registration process. Callback keeps the CloudPolicyClient
  // alive for the length of the registration process.
  registration_helper_ = std::make_unique<CloudPolicyClientRegistrationHelper>(
      policy_client.get(), kCloudPolicyRegistrationType);

  // Using a raw pointer to |this| is okay, because we own the
  // |registration_helper_|.
  auto registration_callback = base::BindOnce(
      &UserPolicySigninService::CallPolicyRegistrationCallback,
      base::Unretained(this), std::move(policy_client), std::move(callback));
  registration_helper_->StartRegistration(identity_manager(), account_id,
                                          std::move(registration_callback));
}

void UserPolicySigninService::CallPolicyRegistrationCallback(
    std::unique_ptr<CloudPolicyClient> client,
    PolicyRegistrationCallback callback) {
  registration_helper_.reset();
  std::move(callback).Run(client->dm_token(), client->client_id());
}

void UserPolicySigninService::Shutdown() {
  // Don't handle ProfileManager when testing because it is null.
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->RemoveObserver(this);
  if (identity_manager())
    identity_manager()->RemoveObserver(this);
  CancelPendingRegistration();
  UserPolicySigninServiceBase::Shutdown();
}

void UserPolicySigninService::OnCloudPolicyServiceInitializationCompleted() {
  UserCloudPolicyManager* manager = policy_manager();
  DCHECK(manager->core()->service()->IsInitializationComplete());
  // The service is now initialized - if the client is not yet registered, then
  // it means that there is no cached policy and so we need to initiate a new
  // client registration.
  if (manager->IsClientRegistered()) {
    DVLOG(1) << "Client already registered - not fetching DMToken";
    return;
  }

  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  base::TimeDelta retry_delay = base::Days(3);
  if (connection_type == net::NetworkChangeNotifier::CONNECTION_ETHERNET ||
      connection_type == net::NetworkChangeNotifier::CONNECTION_WIFI) {
    retry_delay = base::Days(1);
  }

  base::Time last_check_time = base::Time::FromInternalValue(
      profile_prefs_->GetInt64(prefs::kLastPolicyCheckTime));
  base::Time now = base::Time::Now();
  base::Time next_check_time = last_check_time + retry_delay;

  // Check immediately if no check was ever done before (last_check_time == 0),
  // or if the last check was in the future (?), or if we're already past the
  // next check time. Otherwise, delay checking until the next check time.
  base::TimeDelta try_registration_delay = base::Seconds(5);
  if (now > last_check_time && now < next_check_time)
    try_registration_delay = next_check_time - now;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UserPolicySigninService::RegisterCloudPolicyService,
                     weak_factory_.GetWeakPtr()),
      try_registration_delay);
}

void UserPolicySigninService::RegisterCloudPolicyService() {
  // If the user signed-out while this task was waiting then Shutdown() would
  // have been called, which would have invalidated this task. Since we're here
  // then the user must still be signed-in.
  DCHECK(identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  DCHECK(!policy_manager()->IsClientRegistered());
  DCHECK(policy_manager()->core()->client());

  // Persist the current time as the last policy registration attempt time.
  profile_prefs_->SetInt64(prefs::kLastPolicyCheckTime,
                           base::Time::Now().ToInternalValue());

  registration_helper_ = std::make_unique<CloudPolicyClientRegistrationHelper>(
      policy_manager()->core()->client(), kCloudPolicyRegistrationType);
  registration_helper_->StartRegistration(
      identity_manager(),
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync),
      base::BindOnce(&UserPolicySigninService::OnRegistrationDone,
                     base::Unretained(this)));
}

void UserPolicySigninService::CancelPendingRegistration() {
  weak_factory_.InvalidateWeakPtrs();
  registration_helper_.reset();
}

void UserPolicySigninService::OnRegistrationDone() {
  registration_helper_.reset();
}

void UserPolicySigninService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager && IsSignoutEvent(event)) {
    UpdateProfileAttributesWhenSignout(profile_, profile_manager);
    ShutdownUserCloudPolicyManager();
  } else if (IsTurnOffSyncEvent(event)) {
    ShutdownUserCloudPolicyManager();
  }
}

void UserPolicySigninService::OnProfileAdded(Profile* profile) {
  if (profile && profile == profile_)
    InitializeOnProfileReady(profile);
}

void UserPolicySigninService::InitializeOnProfileReady(Profile* profile) {
  DCHECK_EQ(profile, profile_);

  // If using a TestingProfile with no IdentityManager or
  // UserCloudPolicyManager, skip initialization.
  if (!policy_manager() || !identity_manager()) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  // Shutdown the UserCloudPolicyManager when the user signs out. We start
  // observing the IdentityManager here because we don't want to get signout
  // notifications until after the profile has started initializing
  // (http://crbug.com/316229).
  identity_manager()->AddObserver(this);

  AccountId account_id = AccountIdFromAccountInfo(
      identity_manager()->GetPrimaryAccountInfo(consent_level()));
  if (!CanApplyPolicies(/*check_for_refresh_token=*/false)) {
    ShutdownUserCloudPolicyManager();
  } else {
    InitializeForSignedInUser(account_id,
                              profile->GetDefaultStoragePartition()
                                  ->GetURLLoaderFactoryForBrowserProcess());
  }
}

bool UserPolicySigninService::CanApplyPolicies(bool check_for_refresh_token) {
  if (!CanApplyPoliciesForSignedInUser(check_for_refresh_token,
                                       identity_manager())) {
    return false;
  }

  return (profile_can_be_managed_for_testing_ ||
          chrome::enterprise_util::ProfileCanBeManaged(profile_));
}

}  // namespace policy
