// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/constants/ash_paths.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_store.h"
#include "chrome/browser/ash/policy/core/file_util.h"
#include "chrome/browser/ash/policy/external_data/device_local_account_external_data_service.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_cloud_policy_invalidator.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/common/chrome_content_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/invalidation_factory.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace policy {

namespace {

// Cleans up the cache directory by removing subdirectories that are not found
// in |subdirectories_to_keep|. Only caches whose cache directory is found in
// |subdirectories_to_keep| may be running while the clean-up is in progress.
void DeleteOrphanedCaches(const base::FilePath& cache_root_dir,
                          const std::set<std::string>& subdirectories_to_keep) {
  base::FileEnumerator enumerator(cache_root_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string subdirectory(path.BaseName().MaybeAsASCII());
    if (!base::Contains(subdirectories_to_keep, subdirectory)) {
      base::DeletePathRecursively(path);
    }
  }
}

// Removes the subdirectory belonging to |account_id_to_delete| from the cache
// directory. No cache belonging to |account_id_to_delete| may be running while
// the removal is in progress.
void DeleteObsoleteExtensionCache(const std::string& account_id_to_delete) {
  const base::FilePath path =
      base::PathService::CheckedGet(ash::DIR_DEVICE_LOCAL_ACCOUNT_EXTENSIONS)
          .Append(GetUniqueSubDirectoryForAccountID(account_id_to_delete));
  if (base::DirectoryExists(path)) {
    base::DeletePathRecursively(path);
  }
}

}  // namespace

DeviceLocalAccountPolicyService::DeviceLocalAccountPolicyService(
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
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : session_manager_client_(session_manager_client),
      device_settings_service_(device_settings_service),
      cros_settings_(cros_settings),
      invalidation_service_provider_or_listener_(
          invalidation::PointerVariantToRawPointer(
              invalidation_service_provider_or_listener)),
      device_management_service_(nullptr),
      waiting_for_cros_settings_(false),
      orphan_extension_cache_deletion_state_(NOT_STARTED),
      store_background_task_runner_(store_background_task_runner),
      extension_cache_task_runner_(extension_cache_task_runner),
      resource_cache_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      url_loader_factory_(url_loader_factory),
      local_accounts_subscription_(cros_settings_->AddSettingsObserver(
          ash::kAccountsPrefDeviceLocalAccounts,
          base::BindRepeating(
              &DeviceLocalAccountPolicyService::UpdateAccountListIfNonePending,
              base::Unretained(this)))),
      component_policy_cache_root_(base::PathService::CheckedGet(
          ash::DIR_DEVICE_LOCAL_ACCOUNT_COMPONENT_POLICY)) {
  external_data_service_ =
      std::make_unique<DeviceLocalAccountExternalDataService>(
          this, std::move(external_data_service_backend_task_runner));
  UpdateAccountList();
}

DeviceLocalAccountPolicyService::~DeviceLocalAccountPolicyService() {
  DCHECK(policy_brokers_.empty());
}

void DeviceLocalAccountPolicyService::Shutdown() {
  session_manager_client_ = nullptr;
  device_settings_service_ = nullptr;
  cros_settings_ = nullptr;
  device_management_service_ = nullptr;

  // Drop the reference to `invalidation_service_provider_or_listener_` as it
  // may be destroyed sooner than `DeviceLocalAccountPolicyService`.
  std::visit([](auto& v) { v = nullptr; },
             invalidation_service_provider_or_listener_);

  DeleteBrokers(&policy_brokers_);
}

void DeviceLocalAccountPolicyService::Connect(
    DeviceManagementService* device_management_service) {
  DCHECK(!device_management_service_);
  device_management_service_ = device_management_service;

  // Connect the brokers.
  for (auto& [user_id, broker] : policy_brokers_) {
    broker->ConnectIfPossible(device_settings_service_,
                              device_management_service_, url_loader_factory_);
  }
}

DeviceLocalAccountPolicyBroker*
DeviceLocalAccountPolicyService::GetBrokerForUser(std::string_view user_id) {
  PolicyBrokerMap::iterator iter = policy_brokers_.find(user_id);
  if (iter == policy_brokers_.end()) {
    return nullptr;
  }

  return iter->second.get();
}

bool DeviceLocalAccountPolicyService::IsPolicyAvailableForUser(
    std::string_view user_id) {
  DeviceLocalAccountPolicyBroker* broker = GetBrokerForUser(user_id);
  return broker && broker->core()->store()->is_managed();
}

void DeviceLocalAccountPolicyService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceLocalAccountPolicyService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DeviceLocalAccountPolicyService::IsExtensionCacheDirectoryBusy(
    const std::string& account_id) {
  return busy_extension_cache_directories_.find(account_id) !=
         busy_extension_cache_directories_.end();
}

void DeviceLocalAccountPolicyService::StartExtensionCachesIfPossible() {
  for (auto& [user_id, broker] : policy_brokers_) {
    if (!broker->IsCacheRunning() &&
        !IsExtensionCacheDirectoryBusy(broker->account_id())) {
      broker->StartCache(extension_cache_task_runner_);
    }
  }
}

bool DeviceLocalAccountPolicyService::StartExtensionCacheForAccountIfPresent(
    const std::string& account_id) {
  for (auto& [user_id, broker] : policy_brokers_) {
    if (broker->account_id() == account_id) {
      DCHECK(!broker->IsCacheRunning());
      broker->StartCache(extension_cache_task_runner_);
      return true;
    }
  }
  return false;
}

void DeviceLocalAccountPolicyService::OnOrphanedExtensionCachesDeleted() {
  DCHECK_EQ(IN_PROGRESS, orphan_extension_cache_deletion_state_);

  orphan_extension_cache_deletion_state_ = DONE;
  StartExtensionCachesIfPossible();
}

void DeviceLocalAccountPolicyService::OnObsoleteExtensionCacheShutdown(
    const std::string& account_id) {
  DCHECK_NE(NOT_STARTED, orphan_extension_cache_deletion_state_);
  DCHECK(IsExtensionCacheDirectoryBusy(account_id));

  // The account with |account_id| was deleted and the broker for it has shut
  // down completely.

  if (StartExtensionCacheForAccountIfPresent(account_id)) {
    // If another account with the same ID was created in the meantime, its
    // extension cache is started, reusing the cache directory. The directory no
    // longer needs to be marked as busy in this case.
    busy_extension_cache_directories_.erase(account_id);
    return;
  }

  // If no account with |account_id| exists anymore, the cache directory should
  // be removed. The directory must stay marked as busy while the removal is in
  // progress.
  extension_cache_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&DeleteObsoleteExtensionCache, account_id),
      base::BindOnce(
          &DeviceLocalAccountPolicyService::OnObsoleteExtensionCacheDeleted,
          weak_factory_.GetWeakPtr(), account_id));
}

void DeviceLocalAccountPolicyService::OnObsoleteExtensionCacheDeleted(
    const std::string& account_id) {
  DCHECK_EQ(DONE, orphan_extension_cache_deletion_state_);
  DCHECK(IsExtensionCacheDirectoryBusy(account_id));

  // The cache directory for |account_id| has been deleted. The directory no
  // longer needs to be marked as busy.
  busy_extension_cache_directories_.erase(account_id);

  // If another account with the same ID was created in the meantime, start its
  // extension cache, creating a new cache directory.
  StartExtensionCacheForAccountIfPresent(account_id);
}

void DeviceLocalAccountPolicyService::UpdateAccountListIfNonePending() {
  // Avoid unnecessary calls to UpdateAccountList(): If an earlier call is still
  // pending (because the |cros_settings_| are not trusted yet), the updated
  // account list will be processed by that call when it eventually runs.
  if (!waiting_for_cros_settings_) {
    UpdateAccountList();
  }
}

void DeviceLocalAccountPolicyService::UpdateAccountList() {
  ash::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::BindOnce(&DeviceLocalAccountPolicyService::UpdateAccountList,
                         weak_factory_.GetWeakPtr()));
  switch (status) {
    case ash::CrosSettingsProvider::TRUSTED:
      waiting_for_cros_settings_ = false;
      break;
    case ash::CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      waiting_for_cros_settings_ = true;
      // Purposely break to allow initialization with temporarily untrusted
      // settings so that a crash-n-restart public session have its loader
      // properly registered as ExtensionService's external provider.
      break;
    case ash::CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      waiting_for_cros_settings_ = false;
      return;
  }

  // Update |policy_brokers_|, keeping existing entries.
  PolicyBrokerMap old_policy_brokers;
  policy_brokers_.swap(old_policy_brokers);
  std::set<std::string> subdirectories_to_keep;
  const std::vector<DeviceLocalAccount> device_local_accounts =
      GetDeviceLocalAccounts(cros_settings_);
  for (const auto& device_local_account : device_local_accounts) {
    PolicyBrokerMap::iterator broker_it =
        old_policy_brokers.find(device_local_account.user_id);

    std::unique_ptr<DeviceLocalAccountPolicyBroker> broker;
    bool broker_initialized = false;
    if (broker_it != old_policy_brokers.end()) {
      // Reuse the existing broker if present.
      broker = std::move(broker_it->second);
      old_policy_brokers.erase(broker_it);
      broker_initialized = true;
    } else {
      auto store = std::make_unique<DeviceLocalAccountPolicyStore>(
          device_local_account.account_id, session_manager_client_,
          device_settings_service_, store_background_task_runner_);
      scoped_refptr<DeviceLocalAccountExternalDataManager>
          external_data_manager =
              external_data_service_->GetExternalDataManager(
                  device_local_account.account_id, store.get());
      broker = std::make_unique<DeviceLocalAccountPolicyBroker>(
          device_local_account,
          component_policy_cache_root_.Append(GetUniqueSubDirectoryForAccountID(
              device_local_account.account_id)),
          std::move(store), external_data_manager,
          base::BindRepeating(
              &DeviceLocalAccountPolicyService::NotifyPolicyUpdated,
              base::Unretained(this), device_local_account.user_id),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          resource_cache_task_runner_,
          invalidation::RawPointerVariantToPointer(
              invalidation_service_provider_or_listener_));
    }

    // Fire up the cloud connection for fetching policy for the account from
    // the cloud if this is an enterprise-managed device.
    broker->ConnectIfPossible(device_settings_service_,
                              device_management_service_, url_loader_factory_);

    policy_brokers_[device_local_account.user_id] = std::move(broker);
    if (!broker_initialized) {
      // The broker must be initialized after it has been added to
      // |policy_brokers_|.
      policy_brokers_[device_local_account.user_id]->Initialize();
    }

    subdirectories_to_keep.insert(
        GetUniqueSubDirectoryForAccountID(device_local_account.account_id));
  }

  if (orphan_extension_cache_deletion_state_ == NOT_STARTED) {
    DCHECK(old_policy_brokers.empty());
    DCHECK(busy_extension_cache_directories_.empty());

    // If this method is running for the first time, no extension caches have
    // been started yet. Take this opportunity to do a clean-up by removing
    // orphaned cache directories not found in |subdirectories_to_keep| from the
    // cache directory.
    orphan_extension_cache_deletion_state_ = IN_PROGRESS;

    const base::FilePath cache_root_dir =
        base::PathService::CheckedGet(ash::DIR_DEVICE_LOCAL_ACCOUNT_EXTENSIONS);
    extension_cache_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&DeleteOrphanedCaches, cache_root_dir,
                       subdirectories_to_keep),
        base::BindOnce(
            &DeviceLocalAccountPolicyService::OnOrphanedExtensionCachesDeleted,
            weak_factory_.GetWeakPtr()));

    // Start the extension caches for all brokers. These belong to accounts in
    // |account_ids| and are not affected by the clean-up.
    StartExtensionCachesIfPossible();
  } else {
    // If this method has run before, obsolete brokers may exist. Shut down
    // their extension caches and delete the brokers.
    DeleteBrokers(&old_policy_brokers);

    if (orphan_extension_cache_deletion_state_ == DONE) {
      // If the initial clean-up of orphaned cache directories has been
      // complete, start any extension caches that are not running yet but can
      // be started now because their cache directories are not busy.
      StartExtensionCachesIfPossible();
    }
  }

  // Purge the component policy caches of any accounts that have been removed.
  // Do this only after any obsolete brokers have been destroyed. This races
  // with ComponentCloudPolicyStore so make sure they both run on the same task
  // runner.
  resource_cache_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteOrphanedCaches, component_policy_cache_root_,
                     subdirectories_to_keep));

  for (auto& observer : observers_) {
    observer.OnDeviceLocalAccountsChanged();
  }
}

void DeviceLocalAccountPolicyService::DeleteBrokers(PolicyBrokerMap* map) {
  for (auto& [user_id, broker] : *map) {
    if (broker->IsCacheRunning()) {
      DCHECK(!IsExtensionCacheDirectoryBusy(broker->account_id()));
      busy_extension_cache_directories_.insert(broker->account_id());
      broker->StopCache(base::BindOnce(
          &DeviceLocalAccountPolicyService::OnObsoleteExtensionCacheShutdown,
          weak_factory_.GetWeakPtr(), broker->account_id()));
    }
  }
  map->clear();
}

void DeviceLocalAccountPolicyService::NotifyPolicyUpdated(
    const std::string& user_id) {
  for (auto& observer : observers_) {
    observer.OnPolicyUpdated(user_id);
  }
}

}  // namespace policy
