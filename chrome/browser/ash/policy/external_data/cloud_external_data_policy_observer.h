// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_POLICY_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_POLICY_OBSERVER_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/policy/core/common/policy_map.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"

class AccountId;
namespace policy {

// Helper for implementing policies referencing external data: This class
// observes a given |policy_| and fetches the external data that it references
// for all users on the device. Notifications are emitted when an external data
// reference is set, cleared or an external data fetch completes successfully.
//
// State is kept at runtime only: External data references that already exist
// when the class is instantiated are considered new, causing a notification to
// be emitted that an external data reference has been set and the referenced
// external data to be fetched.
class CloudExternalDataPolicyObserver
    : public session_manager::SessionManagerObserver,
      public user_manager::UserManager::Observer,
      public DeviceLocalAccountPolicyService::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked when an external data reference is set for |user_id|.
    virtual void OnExternalDataSet(const std::string& policy,
                                   const std::string& user_id);

    // Invoked when the external data reference is cleared for |user_id|.
    virtual void OnExternalDataCleared(const std::string& policy,
                                       const std::string& user_id);

    // Invoked when the external data referenced for |user_id| has been fetched.
    // Failed fetches are retried and the method is called only when a fetch
    // eventually succeeds. If a fetch fails permanently (e.g. because the
    // external data reference specifies an invalid URL), the method is not
    // called at all.
    virtual void OnExternalDataFetched(const std::string& policy,
                                       const std::string& user_id,
                                       std::unique_ptr<std::string> data,
                                       const base::FilePath& file_path);

    // Removes the data for the given `account_id`.
    virtual void RemoveForAccountId(const AccountId& account_id) = 0;
  };

  // |device_local_account_policy_service| may be nullptr if unavailable (e.g.
  // Active Directory management mode).
  CloudExternalDataPolicyObserver(
      ash::CrosSettings* cros_settings,
      DeviceLocalAccountPolicyService* device_local_account_policy_service,
      const std::string& policy,
      user_manager::UserManager* user_manager,
      std::unique_ptr<Delegate> delegate);

  CloudExternalDataPolicyObserver(const CloudExternalDataPolicyObserver&) =
      delete;
  CloudExternalDataPolicyObserver& operator=(
      const CloudExternalDataPolicyObserver&) = delete;

  ~CloudExternalDataPolicyObserver() override;

  void Init();

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;

  // user_manager::UserManager::Observer:
  void OnUserToBeRemoved(const AccountId& account_id) override;

  // DeviceLocalAccountPolicyService::Observer:
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

  static AccountId GetAccountId(const std::string& user_id);

 private:
  // Helper class that observes |policy_| for a logged-in user.
  class PolicyServiceObserver;

  void RetrieveDeviceLocalAccounts();

  // Handles the new policy map |entry| for |user_id| by canceling any external
  // data fetch currently in progress, emitting a notification that an external
  // data reference has been cleared (if |entry| is NULL) or set (otherwise),
  // starting a new external data fetch in the latter case.
  void HandleExternalDataPolicyUpdate(const std::string& user_id,
                                      const PolicyMap::Entry* entry);

  void OnExternalDataFetched(const std::string& user_id,
                             std::unique_ptr<std::string> data,
                             const base::FilePath& file_path);

  // A map from each device-local account user ID to its current policy map
  // entry for |policy_|.
  typedef std::map<std::string, PolicyMap::Entry> DeviceLocalAccountEntryMap;
  DeviceLocalAccountEntryMap device_local_account_entries_;

  // A map from each logged-in user to the helper that observes |policy_| in the
  // user's PolicyService.
  using LoggedInUserObserverMap =
      std::map<std::string, std::unique_ptr<PolicyServiceObserver>>;
  LoggedInUserObserverMap logged_in_user_observers_;

  raw_ptr<ash::CrosSettings> cros_settings_;
  raw_ptr<DeviceLocalAccountPolicyService> device_local_account_policy_service_;

  // The policy that |this| observes.
  std::string policy_;

  std::unique_ptr<Delegate> delegate_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
  base::CallbackListSubscription device_local_accounts_subscription_;

  // A map from user ID to a base::WeakPtr for each external data fetch
  // currently in progress. This allows fetches to be effectively be canceled by
  // invalidating the pointers.
  using WeakPtrFactory = base::WeakPtrFactory<CloudExternalDataPolicyObserver>;
  using FetchWeakPtrMap =
      std::map<std::string, std::unique_ptr<WeakPtrFactory>>;
  FetchWeakPtrMap fetch_weak_ptrs_;

  base::WeakPtrFactory<CloudExternalDataPolicyObserver> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_CLOUD_EXTERNAL_DATA_POLICY_OBSERVER_H_
