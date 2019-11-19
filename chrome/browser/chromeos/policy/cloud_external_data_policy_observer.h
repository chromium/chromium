// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CLOUD_EXTERNAL_DATA_POLICY_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CLOUD_EXTERNAL_DATA_POLICY_OBSERVER_H_

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/policy/core/common/policy_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

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
    : public content::NotificationObserver,
      public DeviceLocalAccountPolicyService::Observer {
 public:
  class Delegate {
   public:
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

   protected:
    virtual ~Delegate();
  };

  // |device_local_account_policy_service| may be nullptr if unavailable (e.g.
  // Active Directory management mode).
  CloudExternalDataPolicyObserver(
      chromeos::CrosSettings* cros_settings,
      DeviceLocalAccountPolicyService* device_local_account_policy_service,
      const std::string& policy,
      Delegate* delegate);
  ~CloudExternalDataPolicyObserver() override;

  void Init();

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // DeviceLocalAccountPolicyService::Observer:
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

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

  chromeos::CrosSettings* cros_settings_;
  DeviceLocalAccountPolicyService* device_local_account_policy_service_;

  // The policy that |this| observes.
  std::string policy_;

  Delegate* delegate_;

  content::NotificationRegistrar notification_registrar_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      device_local_accounts_subscription_;

  // A map from user ID to a base::WeakPtr for each external data fetch
  // currently in progress. This allows fetches to be effectively be canceled by
  // invalidating the pointers.
  using WeakPtrFactory = base::WeakPtrFactory<CloudExternalDataPolicyObserver>;
  using FetchWeakPtrMap =
      std::map<std::string, std::unique_ptr<WeakPtrFactory>>;
  FetchWeakPtrMap fetch_weak_ptrs_;

  base::WeakPtrFactory<CloudExternalDataPolicyObserver> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CloudExternalDataPolicyObserver);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CLOUD_EXTERNAL_DATA_POLICY_OBSERVER_H_
