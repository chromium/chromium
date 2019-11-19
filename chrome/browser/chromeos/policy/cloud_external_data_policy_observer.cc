// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/cloud_external_data_policy_observer.h"

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/user_manager/user.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace policy {

// Helper class that observes a policy for a logged-in user, notifying the
// |parent_| whenever the external data reference for this user changes.
class CloudExternalDataPolicyObserver::PolicyServiceObserver
    : public PolicyService::Observer {
 public:
  PolicyServiceObserver(CloudExternalDataPolicyObserver* parent,
                        const std::string& user_id,
                        PolicyService* policy_service);
  ~PolicyServiceObserver() override;

  // PolicyService::Observer:
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;

 private:
  CloudExternalDataPolicyObserver* parent_;
  const std::string user_id_;
  PolicyService* policy_service_;

  DISALLOW_COPY_AND_ASSIGN(PolicyServiceObserver);
};

CloudExternalDataPolicyObserver::PolicyServiceObserver::PolicyServiceObserver(
    CloudExternalDataPolicyObserver* parent,
    const std::string& user_id,
    PolicyService* policy_service)
    : parent_(parent),
      user_id_(user_id),
      policy_service_(policy_service) {
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);

  if (!IsDeviceLocalAccountUser(user_id, NULL)) {
    // Notify |parent_| if the external data reference for |user_id_| is set
    // during login. This is omitted for device-local accounts because their
    // policy is available before login and the external data reference will
    // have been seen by the |parent_| already.
    const PolicyMap::Entry* entry = policy_service_->GetPolicies(
        PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
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
    const std::string& user_id) {
}

void CloudExternalDataPolicyObserver::Delegate::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
}

void CloudExternalDataPolicyObserver::Delegate::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {}

CloudExternalDataPolicyObserver::Delegate::~Delegate() {
}

CloudExternalDataPolicyObserver::CloudExternalDataPolicyObserver(
    chromeos::CrosSettings* cros_settings,
    DeviceLocalAccountPolicyService* device_local_account_policy_service,
    const std::string& policy,
    Delegate* delegate)
    : cros_settings_(cros_settings),
      device_local_account_policy_service_(device_local_account_policy_service),
      policy_(policy),
      delegate_(delegate) {
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources());

  if (device_local_account_policy_service_)
    device_local_account_policy_service_->AddObserver(this);

  device_local_accounts_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kAccountsPrefDeviceLocalAccounts,
      base::Bind(&CloudExternalDataPolicyObserver::RetrieveDeviceLocalAccounts,
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

void CloudExternalDataPolicyObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED, type);

  Profile* profile = content::Details<Profile>(details).ptr();
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user) {
    NOTREACHED();
    return;
  }

  const std::string& user_id = user->GetAccountId().GetUserEmail();
  if (base::Contains(logged_in_user_observers_, user_id)) {
    NOTREACHED();
    return;
  }

  ProfilePolicyConnector* policy_connector =
      profile->GetProfilePolicyConnector();
  logged_in_user_observers_[user_id] = std::make_unique<PolicyServiceObserver>(
      this, user_id, policy_connector->policy_service());
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
      HandleExternalDataPolicyUpdate(user_id, NULL);
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

void CloudExternalDataPolicyObserver::RetrieveDeviceLocalAccounts() {
  // Schedule a callback if device policy has not yet been verified.
  if (chromeos::CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(base::Bind(
          &CloudExternalDataPolicyObserver::RetrieveDeviceLocalAccounts,
          weak_factory_.GetWeakPtr()))) {
    return;
  }

  std::vector<DeviceLocalAccount> device_local_account_list =
      policy::GetDeviceLocalAccounts(cros_settings_);
  std::set<std::string> device_local_accounts;
  for (std::vector<DeviceLocalAccount>::const_iterator it =
           device_local_account_list.begin();
       it != device_local_account_list.end(); ++it) {
    device_local_accounts.insert(it->user_id);
  }

  for (DeviceLocalAccountEntryMap::iterator it =
           device_local_account_entries_.begin();
       it != device_local_account_entries_.end(); ) {
    if (!base::Contains(device_local_accounts, it->first)) {
      const std::string user_id = it->first;
      device_local_account_entries_.erase(it++);
      // When a device-local account whose external data reference was set is
      // removed, emit a notification that the external data reference has been
      // cleared.
      HandleExternalDataPolicyUpdate(user_id, NULL);
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
  if (!entry) {
    delegate_->OnExternalDataCleared(policy_, user_id);
    fetch_weak_ptrs_.erase(user_id);
    return;
  }

  delegate_->OnExternalDataSet(policy_, user_id);

  std::unique_ptr<WeakPtrFactory>& weak_ptr_factory = fetch_weak_ptrs_[user_id];
  weak_ptr_factory.reset(new WeakPtrFactory(this));
  if (entry->external_data_fetcher) {
    entry->external_data_fetcher->Fetch(
        base::BindOnce(&CloudExternalDataPolicyObserver::OnExternalDataFetched,
                       weak_ptr_factory->GetWeakPtr(), user_id));
  } else {
    NOTREACHED();
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
