// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account_policy_broker.h"
#include <memory>

#include "ash/constants/ash_paths.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_external_cache.h"
#include "chrome/browser/ash/policy/core/file_util.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "content/public/browser/network_service_instance.h"
#include "extensions/browser/pref_names.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

// Device local accounts are always affiliated.
std::string GetDeviceDMToken(
    ash::DeviceSettingsService* device_settings_service,
    const std::vector<std::string>& user_affiliation_ids) {
  return device_settings_service->policy_data()->request_token();
}

// Creates and initializes a cloud policy client. Returns nullptr if the device
// doesn't have credentials in device settings (i.e. is not
// enterprise-enrolled).
std::unique_ptr<CloudPolicyClient> CreateClient(
    ash::DeviceSettingsService* device_settings_service,
    DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory) {
  const em::PolicyData* policy_data = device_settings_service->policy_data();
  if (!policy_data || !policy_data->has_request_token() ||
      !policy_data->has_device_id() || !device_management_service) {
    return nullptr;
  }

  std::unique_ptr<CloudPolicyClient> client =
      std::make_unique<CloudPolicyClient>(
          device_management_service, system_url_loader_factory,
          base::BindRepeating(&GetDeviceDMToken, device_settings_service));
  std::vector<std::string> user_affiliation_ids(
      policy_data->user_affiliation_ids().begin(),
      policy_data->user_affiliation_ids().end());
  client->SetupRegistration(policy_data->request_token(),
                            policy_data->device_id(), user_affiliation_ids);
  return client;
}

base::Value::Dict GetPrefsFromPolicy(const policy::PolicyMap& policy_map) {
  extensions::ExtensionInstallForceListPolicyHandler policy_handler;
  return policy_handler.GetPolicyDict(policy_map);
}

}  // namespace

DeviceLocalAccountPolicyBroker::DeviceLocalAccountPolicyBroker(
    const DeviceLocalAccount& account,
    const base::FilePath& component_policy_cache_path,
    std::unique_ptr<DeviceLocalAccountPolicyStore> store,
    scoped_refptr<DeviceLocalAccountExternalDataManager> external_data_manager,
    const base::RepeatingClosure& policy_update_callback,
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
  if (account.type != DeviceLocalAccount::TYPE_ARC_KIOSK_APP &&
      account.type != DeviceLocalAccount::TYPE_WEB_KIOSK_APP) {
    extension_tracker_ = std::make_unique<DeviceLocalAccountExtensionTracker>(
        account, store_.get(), &schema_registry_);
  }
  external_cache_ = std::make_unique<chromeos::DeviceLocalAccountExternalCache>(
      user_id_,
      base::PathService::CheckedGet(ash::DIR_DEVICE_LOCAL_ACCOUNT_EXTENSIONS)
          .Append(GetUniqueSubDirectoryForAccountID(account.account_id)));
  store_->AddObserver(this);

  // Unblock the |schema_registry_| so that the |component_policy_service_|
  // starts using it.
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()), GetChromeSchema());
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
    ash::DeviceSettingsService* device_settings_service,
    DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (core_.client()) {
    return;
  }

  std::unique_ptr<CloudPolicyClient> client(CreateClient(
      device_settings_service, device_management_service, url_loader_factory));
  if (!client) {
    return;
  }

  CreateComponentCloudPolicyService(client.get());
  core_.Connect(std::move(client));
  external_data_manager_->Connect(url_loader_factory);
  core_.StartRefreshScheduler();
  UpdateRefreshDelay();
  invalidator_ = std::make_unique<AffiliatedCloudPolicyInvalidator>(
      PolicyInvalidationScope::kDeviceLocalAccount, &core_,
      invalidation_service_provider_, account_id_);
}

void DeviceLocalAccountPolicyBroker::UpdateRefreshDelay() {
  if (core_.refresh_scheduler()) {
    const base::Value* policy_value = store_->policy_map().GetValue(
        key::kPolicyRefreshRate, base::Value::Type::INTEGER);
    if (policy_value) {
      core_.refresh_scheduler()->SetDesiredRefreshDelay(policy_value->GetInt());
    }
  }
}

std::string DeviceLocalAccountPolicyBroker::GetDisplayName() const {
  const base::Value* display_name_value = store_->policy_map().GetValue(
      key::kUserDisplayName, base::Value::Type::STRING);
  if (display_name_value) {
    return display_name_value->GetString();
  }
  return std::string();
}

void DeviceLocalAccountPolicyBroker::OnStoreLoaded(CloudPolicyStore* store) {
  UpdateRefreshDelay();
  UpdateExtensionListFromStore();
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
  std::unique_ptr<ResourceCache> resource_cache(new ResourceCache(
      component_policy_cache_path_, resource_cache_task_runner_,
      /* max_cache_size */ absl::nullopt));

  component_policy_service_ = std::make_unique<ComponentCloudPolicyService>(
      dm_protocol::kChromeExtensionPolicyType, this, &schema_registry_, core(),
      client, std::move(resource_cache), resource_cache_task_runner_);
}

void DeviceLocalAccountPolicyBroker::StartCache(
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner) {
  external_cache_->StartCache(cache_task_runner);
  if (store_->is_initialized()) {
    UpdateExtensionListFromStore();
  }
}

void DeviceLocalAccountPolicyBroker::StopCache(base::OnceClosure callback) {
  external_cache_->StopCache(std::move(callback));
}

bool DeviceLocalAccountPolicyBroker::IsCacheRunning() const {
  return external_cache_->IsCacheRunning();
}

void DeviceLocalAccountPolicyBroker::UpdateExtensionListFromStore() {
  external_cache_->UpdateExtensionsList(
      GetPrefsFromPolicy(store_->policy_map()));
}

base::Value::Dict DeviceLocalAccountPolicyBroker::GetCachedExtensions() const {
  return external_cache_->GetCachedExtensions();
}

}  // namespace policy
