// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/cloud_external_data_policy_observer.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/handlers/configuration_policy_handler_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"

namespace policy {

// Helper class that observes a policy for a logged-in user, notifying the
// |parent_| whenever the external data reference for this user changes.
class CloudExternalDataPolicyObserver::PolicyServiceObserver
    : public PolicyService::Observer {
 public:
  PolicyServiceObserver(CloudExternalDataPolicyObserver* parent,
                        const std::string& user_id,
                        PolicyService* policy_service);

  PolicyServiceObserver(const PolicyServiceObserver&) = delete;
  PolicyServiceObserver& operator=(const PolicyServiceObserver&) = delete;

  ~PolicyServiceObserver() override;

  // PolicyService::Observer:
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;

 private:
  raw_ptr<CloudExternalDataPolicyObserver> parent_;
  const std::string user_id_;
  raw_ptr<PolicyService> policy_service_;
};

CloudExternalDataPolicyObserver::PolicyServiceObserver::PolicyServiceObserver(
    CloudExternalDataPolicyObserver* parent,
    const std::string& user_id,
    PolicyService* policy_service)
    : parent_(parent), user_id_(user_id), policy_service_(policy_service) {
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);

  if (!IsDeviceLocalAccountUser(user_id)) {
    // Notify |parent_| if the external data reference for |user_id_| is set
    // during login. This is omitted for device-local accounts because their
    // policy is available before login and the external data reference will
    // have been seen by the |parent_| already.
    const PolicyMap::Entry* entry =
        policy_service_
            ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
            .Get(parent_->policy_);
    // Notify |parent_| even when |entry| is null (i.e. the policy never existed
    // or once existed but was cleared later).
    parent_->HandleExternalDataPolicyUpdate(user_id_, entry);
  }
}

CloudExternalDataPolicyObserver::PolicyServiceObserver::
    ~PolicyServiceObserver() {
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

void CloudExternalDataPolicyObserver::PolicyServiceObserver::OnPolicyUpdated(
    const PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  DCHECK(ns == PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));

  const PolicyMap::Entry* previous_entry = previous.Get(parent_->policy_);
  const PolicyMap::Entry* current_entry = current.Get(parent_->policy_);
  if ((!previous_entry && current_entry) ||
      (previous_entry && !current_entry) ||
      (previous_entry && current_entry &&
       !previous_entry->Equals(*current_entry))) {
    // Notify |parent_| if the external data reference for |user_id_| has
    // changed.
    parent_->HandleExternalDataPolicyUpdate(user_id_, current_entry);
  }
}

void CloudExternalDataPolicyObserver::Delegate::OnExternalDataSet(
    const std::string& policy,
    const std::string& user_id) {}

void CloudExternalDataPolicyObserver::Delegate::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {}

void CloudExternalDataPolicyObserver::Delegate::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {}

CloudExternalDataPolicyObserver::CloudExternalDataPolicyObserver(
    ash::CrosSettings* cros_settings,
    DeviceLocalAccountPolicyService* device_local_account_policy_service,
    const std::string& policy,
    user_manager::UserManager* user_manager,
    std::unique_ptr<Delegate> delegate)
    : cros_settings_(cros_settings),
      device_local_account_policy_service_(device_local_account_policy_service),
      policy_(policy),
      delegate_(std::move(delegate)) {
  auto* session_manager = session_manager::SessionManager::Get();
  // SessionManager might not exist in unit tests.
  if (session_manager)
    session_observation_.Observe(session_manager);

  user_manager_observation_.Observe(user_manager);

  if (device_local_account_policy_service_)
    device_local_account_policy_service_->AddObserver(this);

  device_local_accounts_subscription_ = cros_settings_->AddSettingsObserver(
      ash::kAccountsPrefDeviceLocalAccounts,
      base::BindRepeating(
          &CloudExternalDataPolicyObserver::RetrieveDeviceLocalAccounts,
          base::Unretained(this)));
}

CloudExternalDataPolicyObserver::~CloudExternalDataPolicyObserver() {
  if (device_local_account_policy_service_)
    device_local_account_policy_service_->RemoveObserver(this);
  device_local_account_entries_.clear();
}

void CloudExternalDataPolicyObserver::Init() {
  RetrieveDeviceLocalAccounts();
}

void CloudExternalDataPolicyObserver::OnUserProfileLoaded(
    const AccountId& account_id) {
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  const std::string& user_id = user->GetAccountId().GetUserEmail();
  if (base::Contains(logged_in_user_observers_, user_id)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  ProfilePolicyConnector* policy_connector =
      profile->GetProfilePolicyConnector();
  logged_in_user_observers_[user_id] = std::make_unique<PolicyServiceObserver>(
      this, user_id, policy_connector->policy_service());
}

void CloudExternalDataPolicyObserver::OnUserToBeRemoved(
    const AccountId& account_id) {
  delegate_->RemoveForAccountId(account_id);
}

void CloudExternalDataPolicyObserver::OnPolicyUpdated(
    const std::string& user_id) {
  if (base::Contains(logged_in_user_observers_, user_id)) {
    // When a device-local account is logged in, a policy change triggers both
    // OnPolicyUpdated() and PolicyServiceObserver::OnPolicyUpdated(). Ignore
    // the former so that the policy change is handled only once.
    return;
  }

  if (!device_local_account_policy_service_) {
    // May happen in tests.
    return;
  }
  DeviceLocalAccountPolicyBroker* broker =
      device_local_account_policy_service_->GetBrokerForUser(user_id);
  if (!broker) {
    // The order in which |this| and the |device_local_account_policy_service_|
    // find out that a new device-local account has been added is undefined. If
    // no |broker| exists yet, the |device_local_account_policy_service_| must
    // not have seen the new |user_id| yet. OnPolicyUpdated() will be invoked
    // again by the |device_local_account_policy_service_| in this case when it
    // finds out about |user_id| and creates a |broker| for it.
    return;
  }

  const PolicyMap::Entry* entry =
      broker->core()->store()->policy_map().Get(policy_);
  if (!entry) {
    DeviceLocalAccountEntryMap::iterator it =
        device_local_account_entries_.find(user_id);
    if (it != device_local_account_entries_.end()) {
      device_local_account_entries_.erase(it);
      HandleExternalDataPolicyUpdate(user_id, nullptr);
    }
    return;
  }

  PolicyMap::Entry& map_entry = device_local_account_entries_[user_id];
  if (map_entry.Equals(*entry))
    return;

  map_entry = entry->DeepCopy();
  HandleExternalDataPolicyUpdate(user_id, entry);
}

void CloudExternalDataPolicyObserver::OnDeviceLocalAccountsChanged() {
  // No action needed here, changes to the list of device-local accounts get
  // handled via the kAccountsPrefDeviceLocalAccounts device setting observer.
}

// static
AccountId CloudExternalDataPolicyObserver::GetAccountId(
    const std::string& user_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  return known_user.GetAccountId(user_id, /*id=*/std::string(),
                                 AccountType::UNKNOWN);
}

void CloudExternalDataPolicyObserver::RetrieveDeviceLocalAccounts() {
  // Schedule a callback if device policy has not yet been verified.
  if (ash::CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(base::BindOnce(
          &CloudExternalDataPolicyObserver::RetrieveDeviceLocalAccounts,
          weak_factory_.GetWeakPtr()))) {
    return;
  }

  std::vector<DeviceLocalAccount> device_local_account_list =
      GetDeviceLocalAccounts(cros_settings_);
  std::set<std::string> device_local_accounts;
  for (std::vector<DeviceLocalAccount>::const_iterator it =
           device_local_account_list.begin();
       it != device_local_account_list.end(); ++it) {
    device_local_accounts.insert(it->user_id);
  }

  for (DeviceLocalAccountEntryMap::iterator it =
           device_local_account_entries_.begin();
       it != device_local_account_entries_.end();) {
    if (!base::Contains(device_local_accounts, it->first)) {
      const std::string user_id = it->first;
      device_local_account_entries_.erase(it++);
      // When a device-local account whose external data reference was set is
      // removed, emit a notification that the external data reference has been
      // cleared.
      HandleExternalDataPolicyUpdate(user_id, nullptr);
    } else {
      ++it;
    }
  }

  for (std::set<std::string>::const_iterator it = device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    OnPolicyUpdated(*it);
  }
}

void CloudExternalDataPolicyObserver::HandleExternalDataPolicyUpdate(
    const std::string& user_id,
    const PolicyMap::Entry* entry) {
  PolicyErrorMap error_map;
  if (!entry || !ExternalDataPolicyHandler::CheckPolicySettings(
                    policy_.c_str(), entry, &error_map)) {
    delegate_->OnExternalDataCleared(policy_, user_id);
    fetch_weak_ptrs_.erase(user_id);
    return;
  }

  delegate_->OnExternalDataSet(policy_, user_id);

  std::unique_ptr<WeakPtrFactory>& weak_ptr_factory = fetch_weak_ptrs_[user_id];
  weak_ptr_factory = std::make_unique<WeakPtrFactory>(this);
  if (entry->external_data_fetcher) {
    entry->external_data_fetcher->Fetch(
        base::BindOnce(&CloudExternalDataPolicyObserver::OnExternalDataFetched,
                       weak_ptr_factory->GetWeakPtr(), user_id));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void CloudExternalDataPolicyObserver::OnExternalDataFetched(
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  FetchWeakPtrMap::iterator it = fetch_weak_ptrs_.find(user_id);
  DCHECK(it != fetch_weak_ptrs_.end());
  fetch_weak_ptrs_.erase(it);
  delegate_->OnExternalDataFetched(policy_, user_id, std::move(data),
                                   file_path);
}

}  // namespace policy
