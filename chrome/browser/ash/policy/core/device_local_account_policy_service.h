// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_broker.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/schema_registry.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH), "For ChromeOS ash-chrome only");

namespace ash {
class DeviceSettingsService;
class SessionManagerClient;
}  // namespace ash

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace invalidation {
class InvalidationListener;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {

class AffiliatedInvalidationServiceProvider;
class DeviceLocalAccountExternalDataService;
class DeviceManagementService;

// Manages user policy blobs for device-local accounts present on the device.
// The actual policy blobs are brokered by session_manager (to prevent file
// manipulation), and we're making signature checks on the policy blobs to
// ensure they're issued by the device owner.
class DeviceLocalAccountPolicyService {
 public:
  // Interface for interested parties to observe policy changes.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Policy for the given |user_id| has changed.
    virtual void OnPolicyUpdated(const std::string& user_id) = 0;

    // The list of accounts has been updated.
    virtual void OnDeviceLocalAccountsChanged() = 0;
  };

  DeviceLocalAccountPolicyService(
      ash::SessionManagerClient* session_manager_client,
      ash::DeviceSettingsService* device_settings_service,
      ash::CrosSettings* cros_settings,
      std::variant<AffiliatedInvalidationServiceProvider*,
                   invalidation::InvalidationListener*>
          invalidation_service_provider_or_listener,
      scoped_refptr<base::SequencedTaskRunner> store_background_task_runner,
      scoped_refptr<base::SequencedTaskRunner> extension_cache_task_runner,
      scoped_refptr<base::SequencedTaskRunner>
          external_data_service_backend_task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  DeviceLocalAccountPolicyService(const DeviceLocalAccountPolicyService&) =
      delete;
  DeviceLocalAccountPolicyService& operator=(
      const DeviceLocalAccountPolicyService&) = delete;

  ~DeviceLocalAccountPolicyService();

  // Shuts down the service and prevents further policy fetches from the cloud.
  void Shutdown();

  // Initializes the cloud policy service connection.
  void Connect(DeviceManagementService* device_management_service);

  // Get the policy broker for a given |user_id|. Returns NULL if that |user_id|
  // does not belong to an existing device-local account.
  DeviceLocalAccountPolicyBroker* GetBrokerForUser(std::string_view user_id);

  // Indicates whether policy has been successfully fetched for the given
  // |user_id|.
  bool IsPolicyAvailableForUser(std::string_view user_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  using PolicyBrokerMap =
      std::map<std::string,
               std::unique_ptr<DeviceLocalAccountPolicyBroker>,
               std::less<>>;

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

  raw_ptr<ash::SessionManagerClient> session_manager_client_;
  raw_ptr<ash::DeviceSettingsService> device_settings_service_;
  raw_ptr<ash::CrosSettings> cros_settings_;
  std::variant<raw_ptr<AffiliatedInvalidationServiceProvider>,
               raw_ptr<invalidation::InvalidationListener>>
      invalidation_service_provider_or_listener_;

  raw_ptr<DeviceManagementService> device_management_service_;

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

  const base::CallbackListSubscription local_accounts_subscription_;

  // Path to the directory that contains the cached policy for components
  // for device-local accounts.
  const base::FilePath component_policy_cache_root_;

  base::WeakPtrFactory<DeviceLocalAccountPolicyService> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
