// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/policy/affiliated_cloud_policy_invalidator.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_external_data_service.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_store.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/common/chrome_content_client.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Device local accounts are always affiliated.
std::string GetDeviceDMToken(
    chromeos::DeviceSettingsService* device_settings_service,
    const std::vector<std::string>& user_affiliation_ids) {
  return device_settings_service->policy_data()->request_token();
}

// Creates and initializes a cloud policy client. Returns nullptr if the device
// doesn't have credentials in device settings (i.e. is not
// enterprise-enrolled).
std::unique_ptr<CloudPolicyClient> CreateClient(
    chromeos::DeviceSettingsService* device_settings_service,
    DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory) {
  const em::PolicyData* policy_data = device_settings_service->policy_data();
  if (!policy_data ||
      !policy_data->has_request_token() ||
      !policy_data->has_device_id() ||
      !device_management_service) {
    return std::unique_ptr<CloudPolicyClient>();
  }

  std::unique_ptr<CloudPolicyClient> client =
      std::make_unique<CloudPolicyClient>(
          std::string() /* machine_id */, std::string() /* machine_model */,
          std::string() /* brand_code */, device_management_service,
          system_url_loader_factory, nullptr /* signing_service */,
          base::BindRepeating(&GetDeviceDMToken, device_settings_service));
  std::vector<std::string> user_affiliation_ids(
      policy_data->user_affiliation_ids().begin(),
      policy_data->user_affiliation_ids().end());
  client->SetupRegistration(policy_data->request_token(),
                            policy_data->device_id(), user_affiliation_ids);
  return client;
}

// Get the subdirectory of the force-installed extension cache and the component
// policy cache used for |account_id|.
std::string GetCacheSubdirectoryForAccountID(const std::string& account_id) {
  return base::HexEncode(account_id.c_str(), account_id.size());
}

// Cleans up the cache directory by removing subdirectories that are not found
// in |subdirectories_to_keep|. Only caches whose cache directory is found in
// |subdirectories_to_keep| may be running while the clean-up is in progress.
void DeleteOrphanedCaches(
    const base::FilePath& cache_root_dir,
    const std::set<std::string>& subdirectories_to_keep) {
  base::FileEnumerator enumerator(cache_root_dir,
                                  false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string subdirectory(path.BaseName().MaybeAsASCII());
    if (!base::ContainsKey(subdirectories_to_keep, subdirectory))
      base::DeleteFile(path, true);
  }
}

// Removes the subdirectory belonging to |account_id_to_delete| from the cache
// directory. No cache belonging to |account_id_to_delete| may be running while
// the removal is in progress.
void DeleteObsoleteExtensionCache(const std::string& account_id_to_delete) {
  base::FilePath cache_root_dir;
  CHECK(base::PathService::Get(chromeos::DIR_DEVICE_LOCAL_ACCOUNT_EXTENSIONS,
                               &cache_root_dir));
  const base::FilePath path = cache_root_dir.Append(
      GetCacheSubdirectoryForAccountID(account_id_to_delete));
  if (base::DirectoryExists(path))
    base::DeleteFile(path, true);
}

}  // namespace

DeviceLocalAccountPolicyBroker::DeviceLocalAccountPolicyBroker(
    const DeviceLocalAccount& account,
    const base::FilePath& component_policy_cache_path,
    std::unique_ptr<DeviceLocalAccountPolicyStore> store,
    scoped_refptr<DeviceLocalAccountExternalDataManager> external_data_manager,
    const base::Closure& policy_update_callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& resource_cache_task_runner,
    AffiliatedInvalidationServiceProvider* invalidation_service_provider)
    : invalidation_service_provider_(invalidation_service_provider),
      account_id_(account.account_id),
      user_id_(account.user_id),
      component_policy_cache_path_(component_policy_cache_path),
      store_(std::move(store)),
      external_data_manager_(external_data_manager),
      core_(dm_protocol::kChromePublicAccountPolicyType,
            store_->account_id(),
            store_.get(),
            task_runner,
            base::BindRepeating(&content::GetNetworkConnectionTracker)),
      policy_update_callback_(policy_update_callback),
      resource_cache_task_runner_(resource_cache_task_runner) {
  if (account.type != DeviceLocalAccount::TYPE_ARC_KIOSK_APP) {
    extension_tracker_.reset(new DeviceLocalAccountExtensionTracker(
        account, store_.get(), &schema_registry_));
  }
  base::FilePath cache_root_dir;
  CHECK(base::PathService::Get(chromeos::DIR_DEVICE_LOCAL_ACCOUNT_EXTENSIONS,
                               &cache_root_dir));
  extension_loader_ = new chromeos::DeviceLocalAccountExternalPolicyLoader(
      store_.get(),
      cache_root_dir.Append(
          GetCacheSubdirectoryForAccountID(account.account_id)));
  store_->AddObserver(this);

  // Unblock the |schema_registry_| so that the |component_policy_service_|
  // starts using it.
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()),
      policy::GetChromeSchema());
  schema_registry_.SetAllDomainsReady();
}

DeviceLocalAccountPolicyBroker::~DeviceLocalAccountPolicyBroker() {
  store_->RemoveObserver(this);
  external_data_manager_->SetPolicyStore(nullptr);
  external_data_manager_->Disconnect();
}

void DeviceLocalAccountPolicyBroker::Initialize() {
  store_->Load();
}

void DeviceLocalAccountPolicyBroker::LoadImmediately() {
  store_->LoadImmediately();
}

bool DeviceLocalAccountPolicyBroker::HasInvalidatorForTest() const {
  return invalidator_ != nullptr;
}

void DeviceLocalAccountPolicyBroker::ConnectIfPossible(
    chromeos::DeviceSettingsService* device_settings_service,
    DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (core_.client())
    return;

  std::unique_ptr<CloudPolicyClient> client(CreateClient(
      device_settings_service, device_management_service, url_loader_factory));
  if (!client)
    return;

  CreateComponentCloudPolicyService(client.get());
  core_.Connect(std::move(client));
  external_data_manager_->Connect(url_loader_factory);
  core_.StartRefreshScheduler();
  UpdateRefreshDelay();
  invalidator_.reset(new AffiliatedCloudPolicyInvalidator(
      em::DeviceRegisterRequest::DEVICE,
      &core_,
      invalidation_service_provider_));
}

void DeviceLocalAccountPolicyBroker::UpdateRefreshDelay() {
  if (core_.refresh_scheduler()) {
    const base::Value* policy_value =
        store_->policy_map().GetValue(key::kPolicyRefreshRate);
    int delay = 0;
    if (policy_value && policy_value->GetAsInteger(&delay))
      core_.refresh_scheduler()->SetDesiredRefreshDelay(delay);
  }
}

std::string DeviceLocalAccountPolicyBroker::GetDisplayName() const {
  std::string display_name;
  const base::Value* display_name_value =
      store_->policy_map().GetValue(policy::key::kUserDisplayName);
  if (display_name_value)
    display_name_value->GetAsString(&display_name);
  return display_name;
}

void DeviceLocalAccountPolicyBroker::OnStoreLoaded(CloudPolicyStore* store) {
  UpdateRefreshDelay();
  policy_update_callback_.Run();
}

void DeviceLocalAccountPolicyBroker::OnStoreError(CloudPolicyStore* store) {
  policy_update_callback_.Run();
}

void DeviceLocalAccountPolicyBroker::OnComponentCloudPolicyUpdated() {
  policy_update_callback_.Run();
}

void DeviceLocalAccountPolicyBroker::CreateComponentCloudPolicyService(
    CloudPolicyClient* client) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableComponentCloudPolicy)) {
    // Disabled via the command line.
    return;
  }

  std::unique_ptr<ResourceCache> resource_cache(new ResourceCache(
      component_policy_cache_path_, resource_cache_task_runner_));

  component_policy_service_.reset(new ComponentCloudPolicyService(
      dm_protocol::kChromeExtensionPolicyType, this, &schema_registry_, core(),
      client, std::move(resource_cache), resource_cache_task_runner_));
}

DeviceLocalAccountPolicyService::DeviceLocalAccountPolicyService(
    chromeos::SessionManagerClient* session_manager_client,
    chromeos::DeviceSettingsService* device_settings_service,
    chromeos::CrosSettings* cros_settings,
    AffiliatedInvalidationServiceProvider* invalidation_service_provider,
    scoped_refptr<base::SequencedTaskRunner> store_background_task_runner,
    scoped_refptr<base::SequencedTaskRunner> extension_cache_task_runner,
    scoped_refptr<base::SequencedTaskRunner>
        external_data_service_backend_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : session_manager_client_(session_manager_client),
      device_settings_service_(device_settings_service),
      cros_settings_(cros_settings),
      invalidation_service_provider_(invalidation_service_provider),
      device_management_service_(nullptr),
      waiting_for_cros_settings_(false),
      orphan_extension_cache_deletion_state_(NOT_STARTED),
      store_background_task_runner_(store_background_task_runner),
      extension_cache_task_runner_(extension_cache_task_runner),
      resource_cache_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      url_loader_factory_(url_loader_factory),
      local_accounts_subscription_(cros_settings_->AddSettingsObserver(
          chromeos::kAccountsPrefDeviceLocalAccounts,
          base::Bind(
              &DeviceLocalAccountPolicyService::UpdateAccountListIfNonePending,
              base::Unretained(this)))),
      weak_factory_(this) {
  CHECK(base::PathService::Get(
      chromeos::DIR_DEVICE_LOCAL_ACCOUNT_COMPONENT_POLICY,
      &component_policy_cache_root_));
  external_data_service_ =
      std::make_unique<DeviceLocalAccountExternalDataService>(
          this, std::move(external_data_service_backend_task_runner));
  UpdateAccountList();
}

DeviceLocalAccountPolicyService::~DeviceLocalAccountPolicyService() {
  DCHECK(policy_brokers_.empty());
}

void DeviceLocalAccountPolicyService::Shutdown() {
  device_management_service_ = nullptr;
  DeleteBrokers(&policy_brokers_);
}

void DeviceLocalAccountPolicyService::Connect(
    DeviceManagementService* device_management_service) {
  DCHECK(!device_management_service_);
  device_management_service_ = device_management_service;

  // Connect the brokers.
  for (PolicyBrokerMap::iterator it(policy_brokers_.begin());
       it != policy_brokers_.end(); ++it) {
    it->second->ConnectIfPossible(device_settings_service_,
                                  device_management_service_,
                                  url_loader_factory_);
  }
}

DeviceLocalAccountPolicyBroker*
    DeviceLocalAccountPolicyService::GetBrokerForUser(
        const std::string& user_id) {
  PolicyBrokerMap::iterator entry = policy_brokers_.find(user_id);
  if (entry == policy_brokers_.end())
    return nullptr;

  return entry->second;
}

bool DeviceLocalAccountPolicyService::IsPolicyAvailableForUser(
    const std::string& user_id) {
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
  for (PolicyBrokerMap::iterator it = policy_brokers_.begin();
       it != policy_brokers_.end(); ++it) {
    if (!it->second->extension_loader()->IsCacheRunning() &&
        !IsExtensionCacheDirectoryBusy(it->second->account_id())) {
      it->second->extension_loader()->StartCache(extension_cache_task_runner_);
    }
  }
}

bool DeviceLocalAccountPolicyService::StartExtensionCacheForAccountIfPresent(
    const std::string& account_id) {
  for (PolicyBrokerMap::iterator it = policy_brokers_.begin();
       it != policy_brokers_.end(); ++it) {
    if (it->second->account_id() == account_id) {
      DCHECK(!it->second->extension_loader()->IsCacheRunning());
      it->second->extension_loader()->StartCache(extension_cache_task_runner_);
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
  if (!waiting_for_cros_settings_)
    UpdateAccountList();
}

void DeviceLocalAccountPolicyService::UpdateAccountList() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::Bind(&DeviceLocalAccountPolicyService::UpdateAccountList,
                     weak_factory_.GetWeakPtr()));
  switch (status) {
    case chromeos::CrosSettingsProvider::TRUSTED:
      waiting_for_cros_settings_ = false;
      break;
    case chromeos::CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      waiting_for_cros_settings_ = true;
      // Purposely break to allow initialization with temporarily untrusted
      // settings so that a crash-n-restart public session have its loader
      // properly registered as ExtensionService's external provider.
      break;
    case chromeos::CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      waiting_for_cros_settings_ = false;
      return;
  }

  // Update |policy_brokers_|, keeping existing entries.
  PolicyBrokerMap old_policy_brokers;
  policy_brokers_.swap(old_policy_brokers);
  std::set<std::string> subdirectories_to_keep;
  const std::vector<DeviceLocalAccount> device_local_accounts =
      GetDeviceLocalAccounts(cros_settings_);
  for (std::vector<DeviceLocalAccount>::const_iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    PolicyBrokerMap::iterator broker_it = old_policy_brokers.find(it->user_id);

    std::unique_ptr<DeviceLocalAccountPolicyBroker> broker;
    bool broker_initialized = false;
    if (broker_it != old_policy_brokers.end()) {
      // Reuse the existing broker if present.
      broker.reset(broker_it->second);
      old_policy_brokers.erase(broker_it);
      broker_initialized = true;
    } else {
      std::unique_ptr<DeviceLocalAccountPolicyStore> store(
          new DeviceLocalAccountPolicyStore(
              it->account_id, session_manager_client_, device_settings_service_,
              store_background_task_runner_));
      scoped_refptr<DeviceLocalAccountExternalDataManager>
          external_data_manager =
              external_data_service_->GetExternalDataManager(it->account_id,
                                                             store.get());
      broker.reset(new DeviceLocalAccountPolicyBroker(
          *it,
          component_policy_cache_root_.Append(
              GetCacheSubdirectoryForAccountID(it->account_id)),
          std::move(store), external_data_manager,
          base::Bind(&DeviceLocalAccountPolicyService::NotifyPolicyUpdated,
                     base::Unretained(this), it->user_id),
          base::ThreadTaskRunnerHandle::Get(), resource_cache_task_runner_,
          invalidation_service_provider_));
    }

    // Fire up the cloud connection for fetching policy for the account from
    // the cloud if this is an enterprise-managed device.
    broker->ConnectIfPossible(device_settings_service_,
                              device_management_service_, url_loader_factory_);

    policy_brokers_[it->user_id] = broker.release();
    if (!broker_initialized) {
      // The broker must be initialized after it has been added to
      // |policy_brokers_|.
      policy_brokers_[it->user_id]->Initialize();
    }

    subdirectories_to_keep.insert(
        GetCacheSubdirectoryForAccountID(it->account_id));
  }

  if (orphan_extension_cache_deletion_state_ == NOT_STARTED) {
    DCHECK(old_policy_brokers.empty());
    DCHECK(busy_extension_cache_directories_.empty());

    // If this method is running for the first time, no extension caches have
    // been started yet. Take this opportunity to do a clean-up by removing
    // orphaned cache directories not found in |subdirectories_to_keep| from the
    // cache directory.
    orphan_extension_cache_deletion_state_ = IN_PROGRESS;

    base::FilePath cache_root_dir;
    CHECK(base::PathService::Get(chromeos::DIR_DEVICE_LOCAL_ACCOUNT_EXTENSIONS,
                                 &cache_root_dir));
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

  for (auto& observer : observers_)
    observer.OnDeviceLocalAccountsChanged();
}

void DeviceLocalAccountPolicyService::DeleteBrokers(PolicyBrokerMap* map) {
  for (PolicyBrokerMap::iterator it = map->begin(); it != map->end(); ++it) {
    scoped_refptr<chromeos::DeviceLocalAccountExternalPolicyLoader>
        extension_loader = it->second->extension_loader();
    if (extension_loader->IsCacheRunning()) {
      DCHECK(!IsExtensionCacheDirectoryBusy(it->second->account_id()));
      busy_extension_cache_directories_.insert(it->second->account_id());
      extension_loader->StopCache(base::Bind(
          &DeviceLocalAccountPolicyService::OnObsoleteExtensionCacheShutdown,
          weak_factory_.GetWeakPtr(),
          it->second->account_id()));
    }

    delete it->second;
  }
  map->clear();
}

void DeviceLocalAccountPolicyService::NotifyPolicyUpdated(
    const std::string& user_id) {
  for (auto& observer : observers_)
    observer.OnPolicyUpdated(user_id);
}

}  // namespace policy
