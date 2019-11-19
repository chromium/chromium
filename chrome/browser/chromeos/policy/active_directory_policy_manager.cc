// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/authpolicy/authpolicy_helper.h"
#include "chrome/browser/chromeos/login/users/affiliation.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/login_manager/policy_descriptor.pb.h"
#include "chromeos/network/onc/variable_expander.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/component_cloud_policy_store.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {
namespace {

// List of policies where variables like ${MACHINE_NAME} should be expanded.
constexpr const char* kPoliciesToExpand[] = {key::kNativePrinters};

// Fetch policy every 90 minutes which matches the Windows default:
// https://technet.microsoft.com/en-us/library/cc940895.aspx
constexpr base::TimeDelta kFetchInterval = base::TimeDelta::FromMinutes(90);

void RunRefreshCallback(base::OnceCallback<void(bool success)> callback,
                        authpolicy::ErrorType error) {
  std::move(callback).Run(error == authpolicy::ERROR_NONE);
}

bool IsComponentPolicyDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableComponentCloudPolicy);
}

}  // namespace

ActiveDirectoryPolicyManager::~ActiveDirectoryPolicyManager() = default;

void ActiveDirectoryPolicyManager::Init(SchemaRegistry* registry) {
  ConfigurationPolicyProvider::Init(registry);

  store_->AddObserver(this);
  if (!store_->is_initialized()) {
    store_->Load();
  }

  // Does nothing if |store_| hasn't yet initialized.
  PublishPolicy();

  scheduler_ = std::make_unique<PolicyScheduler>(
      base::BindRepeating(&ActiveDirectoryPolicyManager::DoPolicyFetch,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ActiveDirectoryPolicyManager::OnPolicyFetched,
                          weak_ptr_factory_.GetWeakPtr()),
      kFetchInterval);

  if (external_data_manager_) {
    // Use the system network context here instead of a context derived from the
    // Profile because Connect() is called before the profile is fully
    // initialized (required so we can perform the initial policy load).
    // Note: The network context can be null for tests and for device policy.
    external_data_manager_->Connect(
        g_browser_process->system_network_context_manager()
            ? g_browser_process->system_network_context_manager()
                  ->GetSharedURLLoaderFactory()
            : nullptr);
  }

  authpolicy_helper_ = std::make_unique<chromeos::AuthPolicyHelper>();
}

void ActiveDirectoryPolicyManager::Shutdown() {
  if (external_data_manager_)
    external_data_manager_->Disconnect();
  extension_policy_service_.reset();
  store_->RemoveObserver(this);
  ConfigurationPolicyProvider::Shutdown();
}

bool ActiveDirectoryPolicyManager::IsInitializationComplete(
    PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return store_->is_initialized();
  if (domain == extension_policy_domain_ && !IsComponentPolicyDisabled()) {
    return extension_policy_service_ &&
           extension_policy_service_->policy() != nullptr;
  }
  return true;
}

void ActiveDirectoryPolicyManager::RefreshPolicies() {
  scheduler_->ScheduleTaskNow();
}

void ActiveDirectoryPolicyManager::OnStoreLoaded(
    CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store_.get(), cloud_policy_store);
  PublishPolicy();
  if (fetch_ever_completed_) {
    // Policy is guaranteed to be up to date with the previous fetch result
    // because OnPolicyFetched() cancels any potentially running Load()
    // operations.
    CancelWaitForInitialPolicy();
  }
}

void ActiveDirectoryPolicyManager::OnStoreError(
    CloudPolicyStore* cloud_policy_store) {
  DCHECK_EQ(store_.get(), cloud_policy_store);
  // Publish policy (even though it hasn't changed) in order to signal load
  // complete on the ConfigurationPolicyProvider interface. Technically, this is
  // only required on the first load, but doesn't hurt in any case.
  PublishPolicy();
  if (fetch_ever_completed_) {
    CancelWaitForInitialPolicy();
  }
}

ActiveDirectoryPolicyManager::ActiveDirectoryPolicyManager(
    std::unique_ptr<CloudPolicyStore> store,
    std::unique_ptr<CloudExternalDataManager> external_data_manager,
    PolicyDomain extension_policy_domain)
    : store_(std::move(store)),
      external_data_manager_(std::move(external_data_manager)),
      extension_policy_domain_(extension_policy_domain) {
  DCHECK(extension_policy_domain_ == POLICY_DOMAIN_EXTENSIONS ||
         extension_policy_domain_ == POLICY_DOMAIN_SIGNIN_EXTENSIONS);
}

void ActiveDirectoryPolicyManager::OnComponentActiveDirectoryPolicyUpdated() {
  PublishPolicy();
}

void ActiveDirectoryPolicyManager::PublishPolicy() {
  if (!store_->is_initialized())
    return;
  OnPublishPolicy();

  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();
  PolicyMap& policy_map =
      bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  policy_map.CopyFrom(store_->policy_map());
  if (extension_policy_service_ && extension_policy_service_->policy())
    bundle->MergeFrom(*extension_policy_service_->policy());

  // Overwrite the source which is POLICY_SOURCE_CLOUD by default.
  // TODO(tnagel): Rename CloudPolicyStore to PolicyStore and make the source
  // configurable, then drop PolicyMap::SetSourceForAll().
  policy_map.SetSourceForAll(POLICY_SOURCE_ACTIVE_DIRECTORY);
  SetEnterpriseUsersDefaults(&policy_map);

  // Expand e.g. ${MACHINE_NAME} for a selected set of policies.
  ExpandVariables(&policy_map);

  // Policy is ready, send it off.
  UpdatePolicy(std::move(bundle));
}

void ActiveDirectoryPolicyManager::CreateExtensionPolicyService(
    PolicyScope scope,
    login_manager::PolicyAccountType account_type,
    const AccountId& account_id,
    SchemaRegistry* schema_registry) {
  if (IsComponentPolicyDisabled())
    return;

  std::string cryptohome_id;
  if (!account_id.empty())
    cryptohome_id = cryptohome::Identification(account_id).id();

  // Create the service for sign-in extensions (device scope) or user profile
  // extensions (user scope).
  DCHECK(!extension_policy_service_);
  extension_policy_service_ =
      std::make_unique<ComponentActiveDirectoryPolicyService>(
          scope, extension_policy_domain_, account_type, cryptohome_id, this,
          schema_registry);
}

void ActiveDirectoryPolicyManager::OnPolicyFetched(bool success) {
  fetch_ever_completed_ = true;
  // In case of failure try to proceed with cached policy.
  if (!success && store()->is_initialized())
    CancelWaitForInitialPolicy();
  // Load/retrieve independently of success or failure to keep in sync with the
  // state in session manager. This cancels any potentially running Load()
  // operations thus it is guaranteed that at the next OnStoreLoaded()
  // invocation the policy is up-to-date with what was fetched.
  store_->Load();
  if (extension_policy_service_)
    extension_policy_service_->RetrievePolicies();
}

void ActiveDirectoryPolicyManager::ExpandVariables(PolicyMap* policy_map) {
  const em::PolicyData* policy = store_->policy();
  if (!policy || policy_map->empty())
    return;
  if (policy->machine_name().empty()) {
    LOG(ERROR) << "Cannot expand machine_name (empty string in policy)";
    return;
  }

  chromeos::VariableExpander expander(
      {{"MACHINE_NAME", policy->machine_name()}});
  for (const char* policy_name : kPoliciesToExpand) {
    base::Value* value = policy_map->GetMutableValue(policy_name);
    if (value) {
      if (!expander.ExpandValue(value)) {
        LOG(ERROR) << "Failed to expand at least one variable in policy "
                   << policy_name;
      }
    }
  }
}

UserActiveDirectoryPolicyManager::UserActiveDirectoryPolicyManager(
    const AccountId& account_id,
    bool policy_required,
    base::TimeDelta initial_policy_fetch_timeout,
    base::OnceClosure exit_session,
    std::unique_ptr<CloudPolicyStore> store,
    std::unique_ptr<CloudExternalDataManager> external_data_manager)
    : ActiveDirectoryPolicyManager(
          std::move(store),
          std::move(external_data_manager),
          POLICY_DOMAIN_EXTENSIONS /* extension_policy_domain */),
      account_id_(account_id),
      policy_required_(policy_required),
      waiting_for_initial_policy_fetch_(
          !initial_policy_fetch_timeout.is_zero()),
      exit_session_(std::move(exit_session)) {
  DCHECK(!initial_policy_fetch_timeout.is_max());
  // Delaying initialization complete is intended for user policy only.
  if (waiting_for_initial_policy_fetch_) {
    initial_policy_timeout_.Start(
        FROM_HERE, initial_policy_fetch_timeout,
        base::Bind(&UserActiveDirectoryPolicyManager::OnBlockingFetchTimeout,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

UserActiveDirectoryPolicyManager::~UserActiveDirectoryPolicyManager() = default;

void UserActiveDirectoryPolicyManager::Init(SchemaRegistry* registry) {
  DCHECK(store()->is_initialized() || waiting_for_initial_policy_fetch_ ||
         !policy_required_ /* policy may not be required in tests */);
  if (store()->is_initialized() && !store()->has_policy() && policy_required_) {
    // Exit the session in case of immediate load if policy is required.
    LOG(ERROR) << "Policy from forced immediate load could not be obtained. "
               << "Aborting profile initialization";
    if (exit_session_)
      std::move(exit_session_).Run();
  }
  ActiveDirectoryPolicyManager::Init(registry);

  // Create the extension policy handler here. This is different from the device
  // policy manager, which can't do this in Init() because it needs to wait for
  // the sign-in profile's schema registry.
  CreateExtensionPolicyService(POLICY_SCOPE_USER,
                               login_manager::ACCOUNT_TYPE_USER, account_id_,
                               registry);
}

bool UserActiveDirectoryPolicyManager::IsInitializationComplete(
    PolicyDomain domain) const {
  if (waiting_for_initial_policy_fetch_)
    return false;

  return ActiveDirectoryPolicyManager::IsInitializationComplete(domain);
}

void UserActiveDirectoryPolicyManager::ForceTimeoutForTesting() {
  DCHECK(initial_policy_timeout_.IsRunning());
  // Stop the timer to mimic what happens when a real timer fires, then invoke
  // the timer callback directly.
  initial_policy_timeout_.Stop();
  OnBlockingFetchTimeout();
}

void UserActiveDirectoryPolicyManager::DoPolicyFetch(
    PolicyScheduler::TaskCallback callback) {
  authpolicy_helper()->RefreshUserPolicy(
      account_id_, base::BindOnce(&RunRefreshCallback, std::move(callback)));
}

void UserActiveDirectoryPolicyManager::CancelWaitForInitialPolicy() {
  if (!waiting_for_initial_policy_fetch_)
    return;

  initial_policy_timeout_.Stop();

  // If the conditions to continue profile initialization are not met, the user
  // session is exited and initialization is not set as completed.
  if (!store()->has_policy() && policy_required_) {
    // If there's no policy at all (not even cached), but policy is required,
    // the user session must not continue.
    LOG(ERROR) << "Policy could not be obtained. "
               << "Aborting profile initialization";
    if (exit_session_)
      std::move(exit_session_).Run();
    return;
  }

  // Set initialization complete.
  waiting_for_initial_policy_fetch_ = false;

  // Publish policy (even though it hasn't changed) in order to signal load
  // complete on the ConfigurationPolicyProvider interface.
  PublishPolicy();
}

void UserActiveDirectoryPolicyManager::OnPublishPolicy() {
  const em::PolicyData* policy_data = store()->policy();
  if (!policy_data)
    return;

  // Update user affiliation IDs.
  chromeos::AffiliationIDSet set_of_user_affiliation_ids(
      policy_data->user_affiliation_ids().begin(),
      policy_data->user_affiliation_ids().end());

  chromeos::ChromeUserManager::Get()->SetUserAffiliation(
      account_id_, set_of_user_affiliation_ids);
}

void UserActiveDirectoryPolicyManager::OnBlockingFetchTimeout() {
  DCHECK(waiting_for_initial_policy_fetch_);
  LOG(WARNING) << "Timed out while waiting for the policy fetch. "
               << "The session will start with the cached policy.";
  if ((fetch_ever_completed_ && !store()->is_initialized()) ||
      (!fetch_ever_completed_ && !store()->has_policy())) {
    // Waiting for store to load if policy was fetched. Or for policy fetch to
    // complete if there is no cached policy.
    return;
  }
  CancelWaitForInitialPolicy();
}

DeviceActiveDirectoryPolicyManager::DeviceActiveDirectoryPolicyManager(
    std::unique_ptr<CloudPolicyStore> store)
    : ActiveDirectoryPolicyManager(
          std::move(store),
          nullptr /* external_data_manager */,
          POLICY_DOMAIN_SIGNIN_EXTENSIONS /* extension_policy_domain */) {}

void DeviceActiveDirectoryPolicyManager::Shutdown() {
  ActiveDirectoryPolicyManager::Shutdown();
  signin_profile_forwarding_schema_registry_.reset();
}

void DeviceActiveDirectoryPolicyManager::SetSigninProfileSchemaRegistry(
    SchemaRegistry* schema_registry) {
  DCHECK(!signin_profile_forwarding_schema_registry_);
  signin_profile_forwarding_schema_registry_ =
      std::make_unique<ForwardingSchemaRegistry>(schema_registry);

  CreateExtensionPolicyService(
      POLICY_SCOPE_MACHINE, login_manager::ACCOUNT_TYPE_DEVICE,
      EmptyAccountId(), signin_profile_forwarding_schema_registry_.get());
}

DeviceActiveDirectoryPolicyManager::~DeviceActiveDirectoryPolicyManager() =
    default;

void DeviceActiveDirectoryPolicyManager::DoPolicyFetch(
    base::OnceCallback<void(bool success)> callback) {
  authpolicy_helper()->RefreshDevicePolicy(
      base::BindOnce(&RunRefreshCallback, std::move(callback)));
}

}  // namespace policy
