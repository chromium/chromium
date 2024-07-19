// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_BROKER_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_BROKER_H_

#include <memory>
#include <string>
#include <variant>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_extension_tracker.h"
#include "chrome/browser/ash/policy/core/device_local_account_external_cache.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_store.h"
#include "chrome/browser/ash/policy/external_data/device_local_account_external_data_manager.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_cloud_policy_invalidator.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/component_cloud_policy_service.h"
#include "components/policy/core/common/cloud/device_management_service.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH), "For ChromeOS ash-chrome only");

namespace chromeos {
class DeviceLocalAccountExternalPolicyLoader;
}  // namespace chromeos

namespace invalidation {
class InvalidationListener;
}

namespace policy {

class AffiliatedInvalidationServiceProvider;

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
      const base::RepeatingClosure& policy_updated_callback,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const scoped_refptr<base::SequencedTaskRunner>&
          resource_cache_task_runner,
      std::variant<AffiliatedInvalidationServiceProvider*,
                   invalidation::InvalidationListener*>
          invalidation_service_provider_or_listener);

  DeviceLocalAccountPolicyBroker(const DeviceLocalAccountPolicyBroker&) =
      delete;
  DeviceLocalAccountPolicyBroker& operator=(
      const DeviceLocalAccountPolicyBroker&) = delete;

  ~DeviceLocalAccountPolicyBroker() override;

  // Initialize the broker, start asynchronous load of its |store_|.
  void Initialize();

  // Loads store synchronously.
  void LoadImmediately();

  // For the difference between |account_id| and |user_id|, see the
  // documentation of DeviceLocalAccount.
  const std::string& account_id() const { return account_id_; }
  const std::string& user_id() const { return user_id_; }

  scoped_refptr<extensions::ExternalLoader> extension_loader() const;

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
      ash::DeviceSettingsService* device_settings_service,
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

  // Start the cache using the supplied |cache_task_runner|.
  void StartCache(
      const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner);

  // Stop the cache. When the cache is stopped, |callback| will be invoked.
  void StopCache(base::OnceClosure callback);

  // Return whether the cache is currently running.
  bool IsCacheRunning() const;

  // Returns all cached extensions, both the ones meant for Ash and the ones
  // meant for Lacros.
  base::Value::Dict GetCachedExtensionsForTesting() const;

 private:
  void CreateComponentCloudPolicyService(CloudPolicyClient* client);
  void UpdateExtensionListFromStore();

  const std::variant<raw_ptr<AffiliatedInvalidationServiceProvider>,
                     raw_ptr<invalidation::InvalidationListener>>
      invalidation_service_provider_or_listener_;
  const std::string account_id_;
  const std::string user_id_;
  const base::FilePath component_policy_cache_path_;
  SchemaRegistry schema_registry_;
  const std::unique_ptr<DeviceLocalAccountPolicyStore> store_;
  std::unique_ptr<DeviceLocalAccountExtensionTracker> extension_tracker_;
  scoped_refptr<DeviceLocalAccountExternalDataManager> external_data_manager_;
  scoped_refptr<chromeos::DeviceLocalAccountExternalPolicyLoader>
      extension_loader_;
  std::unique_ptr<chromeos::DeviceLocalAccountExternalCache> external_cache_;
  CloudPolicyCore core_;
  std::unique_ptr<ComponentCloudPolicyService> component_policy_service_;
  base::RepeatingClosure policy_update_callback_;
  std::variant<std::unique_ptr<AffiliatedCloudPolicyInvalidator>,
               std::unique_ptr<CloudPolicyInvalidator>>
      invalidator_ = std::unique_ptr<AffiliatedCloudPolicyInvalidator>{nullptr};
  const scoped_refptr<base::SequencedTaskRunner> resource_cache_task_runner_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_POLICY_BROKER_H_
