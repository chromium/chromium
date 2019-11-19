// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/extensions/device_local_account_external_policy_loader.h"
#include "chrome/browser/chromeos/policy/device_local_account_extension_tracker.h"
#include "chrome/browser/chromeos/policy/device_local_account_external_data_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/component_cloud_policy_service.h"
#include "components/policy/core/common/schema_registry.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {
class DeviceSettingsService;
class SessionManagerClient;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class AffiliatedCloudPolicyInvalidator;
class AffiliatedInvalidationServiceProvider;
struct DeviceLocalAccount;
class DeviceLocalAccountExternalDataService;
class DeviceLocalAccountPolicyStore;
class DeviceManagementService;

// The main switching central that downloads, caches, refreshes, etc. policy for
// a single device-local account.
class DeviceLocalAccountPolicyBroker
    : public CloudPolicyStore::Observer,
      public ComponentCloudPolicyService::Delegate {
 public:
  // |invalidation_service_provider| must outlive |this|.
  // |policy_update_callback| will be invoked to notify observers that the
  // policy for |account| has been updated.
  // |task_runner| is the runner for policy refresh tasks.
  // |resource_cache_task_runner| is the task runner used for file operations,
  // it must be sequenced together with other tasks running on the same files.
  DeviceLocalAccountPolicyBroker(
      const DeviceLocalAccount& account,
      const base::FilePath& component_policy_cache_path,
      std::unique_ptr<DeviceLocalAccountPolicyStore> store,
      scoped_refptr<DeviceLocalAccountExternalDataManager>
          external_data_manager,
      const base::Closure& policy_updated_callback,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const scoped_refptr<base::SequencedTaskRunner>&
          resource_cache_task_runner,
      AffiliatedInvalidationServiceProvider* invalidation_service_provider);
  ~DeviceLocalAccountPolicyBroker() override;

  // Initialize the broker, start asynchronous load of its |store_|.
  void Initialize();

  // Loads store synchronously.
  void LoadImmediately();

  // For the difference between |account_id| and |user_id|, see the
  // documentation of DeviceLocalAccount.
  const std::string& account_id() const { return account_id_; }
  const std::string& user_id() const { return user_id_; }

  scoped_refptr<chromeos::DeviceLocalAccountExternalPolicyLoader>
      extension_loader() const { return extension_loader_; }

  CloudPolicyCore* core() { return &core_; }
  const CloudPolicyCore* core() const { return &core_; }

  scoped_refptr<DeviceLocalAccountExternalDataManager> external_data_manager() {
    return external_data_manager_;
  }

  ComponentCloudPolicyService* component_policy_service() const {
    return component_policy_service_.get();
  }

  SchemaRegistry* schema_registry() { return &schema_registry_; }

  bool HasInvalidatorForTest() const;

  // Fire up the cloud connection for fetching policy for the account from the
  // cloud if this is an enterprise-managed device.
  void ConnectIfPossible(
      chromeos::DeviceSettingsService* device_settings_service,
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Reads the refresh delay from policy and configures the refresh scheduler.
  void UpdateRefreshDelay();

  // Retrieves the display name for the account as stored in policy. Returns an
  // empty string if the policy is not present.
  std::string GetDisplayName() const;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  // ComponentCloudPolicyService::Delegate:
  void OnComponentCloudPolicyUpdated() override;

 private:
  void CreateComponentCloudPolicyService(CloudPolicyClient* client);

  AffiliatedInvalidationServiceProvider* const invalidation_service_provider_;
  const std::string account_id_;
  const std::string user_id_;
  const base::FilePath component_policy_cache_path_;
  SchemaRegistry schema_registry_;
  const std::unique_ptr<DeviceLocalAccountPolicyStore> store_;
  std::unique_ptr<DeviceLocalAccountExtensionTracker> extension_tracker_;
  scoped_refptr<DeviceLocalAccountExternalDataManager> external_data_manager_;
  scoped_refptr<chromeos::DeviceLocalAccountExternalPolicyLoader>
      extension_loader_;
  CloudPolicyCore core_;
  std::unique_ptr<ComponentCloudPolicyService> component_policy_service_;
  base::Closure policy_update_callback_;
  std::unique_ptr<AffiliatedCloudPolicyInvalidator> invalidator_;
  const scoped_refptr<base::SequencedTaskRunner> resource_cache_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyBroker);
};

// Manages user policy blobs for device-local accounts present on the device.
// The actual policy blobs are brokered by session_manager (to prevent file
// manipulation), and we're making signature checks on the policy blobs to
// ensure they're issued by the device owner.
class DeviceLocalAccountPolicyService {
 public:
  // Interface for interested parties to observe policy changes.
  class Observer {
   public:
    virtual ~Observer() {}

    // Policy for the given |user_id| has changed.
    virtual void OnPolicyUpdated(const std::string& user_id) = 0;

    // The list of accounts has been updated.
    virtual void OnDeviceLocalAccountsChanged() = 0;
  };

  DeviceLocalAccountPolicyService(
      chromeos::SessionManagerClient* session_manager_client,
      chromeos::DeviceSettingsService* device_settings_service,
      chromeos::CrosSettings* cros_settings,
      AffiliatedInvalidationServiceProvider* invalidation_service_provider,
      scoped_refptr<base::SequencedTaskRunner> store_background_task_runner,
      scoped_refptr<base::SequencedTaskRunner> extension_cache_task_runner,
      scoped_refptr<base::SequencedTaskRunner>
          external_data_service_backend_task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  virtual ~DeviceLocalAccountPolicyService();

  // Shuts down the service and prevents further policy fetches from the cloud.
  void Shutdown();

  // Initializes the cloud policy service connection.
  void Connect(DeviceManagementService* device_management_service);

  // Get the policy broker for a given |user_id|. Returns NULL if that |user_id|
  // does not belong to an existing device-local account.
  DeviceLocalAccountPolicyBroker* GetBrokerForUser(const std::string& user_id);

  // Indicates whether policy has been successfully fetched for the given
  // |user_id|.
  bool IsPolicyAvailableForUser(const std::string& user_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  typedef std::map<std::string, DeviceLocalAccountPolicyBroker*>
      PolicyBrokerMap;

  // Returns |true| if the directory in which force-installed extensions are
  // cached for |account_id| is busy, either because a broker that was using
  // this directory has not shut down completely yet or because the directory is
  // being deleted.
  bool IsExtensionCacheDirectoryBusy(const std::string& account_id);

  // Starts any extension caches that are not running yet but can be started now
  // because their cache directories are no longer busy.
  void StartExtensionCachesIfPossible();

  // Checks whether a broker exists for |account_id|. If so, starts the broker's
  // extension cache and returns |true|. Otherwise, returns |false|.
  bool StartExtensionCacheForAccountIfPresent(const std::string& account_id);

  // Called back when any extension caches belonging to device-local accounts
  // that no longer exist have been removed at start-up.
  void OnOrphanedExtensionCachesDeleted();

  // Called back when the extension cache for |account_id| has been shut down.
  void OnObsoleteExtensionCacheShutdown(const std::string& account_id);

  // Called back when the extension cache for |account_id| has been removed.
  void OnObsoleteExtensionCacheDeleted(const std::string& account_id);

  // Re-queries the list of defined device-local accounts from device settings
  // and updates |policy_brokers_| to match that list.
  void UpdateAccountList();

  // Calls |UpdateAccountList| if there are no previous calls pending.
  void UpdateAccountListIfNonePending();

  // Deletes brokers in |map| and clears it.
  void DeleteBrokers(PolicyBrokerMap* map);

  // Notifies the |observers_| that the policy for |user_id| has changed.
  void NotifyPolicyUpdated(const std::string& user_id);

  base::ObserverList<Observer, true>::Unchecked observers_;

  chromeos::SessionManagerClient* session_manager_client_;
  chromeos::DeviceSettingsService* device_settings_service_;
  chromeos::CrosSettings* cros_settings_;
  AffiliatedInvalidationServiceProvider* invalidation_service_provider_;

  DeviceManagementService* device_management_service_;

  // The device-local account policy brokers, keyed by user ID.
  PolicyBrokerMap policy_brokers_;

  // Whether a call to UpdateAccountList() is pending because |cros_settings_|
  // are not trusted yet.
  bool waiting_for_cros_settings_;

  // Orphaned extension caches are removed at startup. This tracks the status of
  // that process.
  enum OrphanExtensionCacheDeletionState {
    NOT_STARTED,
    IN_PROGRESS,
    DONE,
  };
  OrphanExtensionCacheDeletionState orphan_extension_cache_deletion_state_;

  // Account IDs whose extension cache directories are busy, either because a
  // broker for the account has not shut down completely yet or because the
  // directory is being deleted.
  std::set<std::string> busy_extension_cache_directories_;

  const scoped_refptr<base::SequencedTaskRunner> store_background_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> extension_cache_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> resource_cache_task_runner_;

  std::unique_ptr<DeviceLocalAccountExternalDataService> external_data_service_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      local_accounts_subscription_;

  // Path to the directory that contains the cached policy for components
  // for device-local accounts.
  base::FilePath component_policy_cache_root_;

  base::WeakPtrFactory<DeviceLocalAccountPolicyService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
