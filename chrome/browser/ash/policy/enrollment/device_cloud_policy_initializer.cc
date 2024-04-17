// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/device_cloud_policy_initializer.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_client_factory_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_content_client.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace policy {

DeviceCloudPolicyInitializer::DeviceCloudPolicyInitializer(
    DeviceManagementService* enterprise_service,
    ash::InstallAttributes* install_attributes,
    ServerBackedStateKeysBroker* state_keys_broker,
    DeviceCloudPolicyStoreAsh* policy_store,
    DeviceCloudPolicyManagerAsh* policy_manager,
    ash::system::StatisticsProvider* statistics_provider)
    : enterprise_service_(enterprise_service),
      install_attributes_(install_attributes),
      state_keys_broker_(state_keys_broker),
      policy_store_(policy_store),
      policy_manager_(policy_manager),
      statistics_provider_(statistics_provider) {}

void DeviceCloudPolicyInitializer::SetSystemURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory) {
  system_url_loader_factory_for_testing_ = system_url_loader_factory;
}

DeviceCloudPolicyInitializer::~DeviceCloudPolicyInitializer() {
  DCHECK(!is_initialized_);
}

void DeviceCloudPolicyInitializer::Init() {
  DCHECK(!is_initialized_);

  is_initialized_ = true;

  policy_store_->AddObserver(this);
  policy_manager_observer_.Observe(policy_manager_.get());

  // If FRE is enabled, we want to obtain state keys before proceeding.
  if (AutoEnrollmentTypeChecker::IsFREEnabled()) {
  state_keys_update_subscription_ =
      state_keys_broker_->RegisterUpdateCallback(base::BindRepeating(
          &DeviceCloudPolicyInitializer::TryToStartConnection,
          base::Unretained(this)));
  }

  // TODO(b/333951800): This could be an else to the if, check if warranted.
  TryToStartConnection();
}

void DeviceCloudPolicyInitializer::Shutdown() {
  DCHECK(is_initialized_);

  policy_store_->RemoveObserver(this);
  state_keys_update_subscription_ = {};
  policy_manager_observer_.Reset();
  is_initialized_ = false;
}

void DeviceCloudPolicyInitializer::OnStoreLoaded(CloudPolicyStore* store) {
  TryToStartConnection();
}

void DeviceCloudPolicyInitializer::OnStoreError(CloudPolicyStore* store) {
  // Do nothing.
}

void DeviceCloudPolicyInitializer::OnDeviceCloudPolicyManagerConnected() {
  // Do nothing.
}
void DeviceCloudPolicyInitializer::OnDeviceCloudPolicyManagerGotRegistry() {
  // `policy_manager_->HasSchemaRegistry()` is one of requirements for
  // StartConnection. Make another attempt when `policy_manager_` gets its
  // registry.
  policy_manager_observer_.Reset();
  TryToStartConnection();
}

std::unique_ptr<CloudPolicyClient> DeviceCloudPolicyInitializer::CreateClient(
    DeviceManagementService* device_management_service) {
  // DeviceDMToken callback is empty here because for device policies this
  // DMToken is already provided in the policy fetch requests.
  return CreateDeviceCloudPolicyClientAsh(
      statistics_provider_, device_management_service,
      system_url_loader_factory_for_testing_
          ? system_url_loader_factory_for_testing_
          : g_browser_process->shared_url_loader_factory(),
      CloudPolicyClient::DeviceDMTokenCallback());
}

void DeviceCloudPolicyInitializer::TryToStartConnection() {
  if (!policy_store_->is_initialized() || !policy_store_->has_policy()) {
    return;
  }

  if (!policy_manager_store_ready_notified_) {
    policy_manager_store_ready_notified_ = true;
    policy_manager_->OnPolicyStoreReady(install_attributes_);
  }

  // TODO(crbug.com/1304636): Move this and all other checks from here to a
  // separate method.
  if (!policy_manager_->HasSchemaRegistry()) {
    // crbug.com/1295871: `policy_manager_` might not have schema registry on
    // start connection attempt. This may happen on chrome restart when
    // `chrome::kInitialProfile` is created after login profile: policy will be
    // loaded but `BuildSchemaRegistryServiceForProfile` will not be called for
    // non-initial / non-sign-in profile.
    return;
  }

  // TODO(b/181140445): If we had a separate state keys upload request to DM
  // Server we could drop the `state_keys_broker_->available()` requirement.
  if (state_keys_broker_->available() ||
      !AutoEnrollmentTypeChecker::IsFREEnabled()) {
    StartConnection(CreateClient(enterprise_service_));
  }
}

void DeviceCloudPolicyInitializer::StartConnection(
    std::unique_ptr<CloudPolicyClient> client) {
  // This initializer will be deleted once `policy_manager_` is connected.
  // Stop observing the manager as there's nothing interesting it can say
  // anymore.
  policy_manager_observer_.Reset();

  if (!policy_manager_->IsConnected()) {
    policy_manager_->StartConnection(std::move(client), install_attributes_);
  }
}

}  // namespace policy
