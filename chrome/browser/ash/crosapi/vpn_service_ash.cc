// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/vpn_service_ash.h"

#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_driver_client.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_observer.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "crypto/sha2.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace {

using SuccessOrFailureCallback =
    base::OnceCallback<void(crosapi::mojom::VpnErrorResponsePtr)>;

void RunSuccessCallback(SuccessOrFailureCallback callback) {
  std::move(callback).Run(nullptr);
}

void RunFailureCallback(SuccessOrFailureCallback callback,
                        const std::string& error_name,
                        const std::string& error_message) {
  std::move(callback).Run(
      crosapi::mojom::VpnErrorResponse::New(error_name, error_message));
}

// crosapi::mojom::VpnService expects a single callback, whereas the API is
// designed to pass in two (one for success, one for failure). This function
// unpacks (splits) the single callback into the success and failure ones
// respectively. For the reverse transformation see
// chrome/browser/chromeos/extensions/vpn_provider/vpn_service.cc
std::pair<crosapi::VpnServiceForExtensionAsh::SuccessCallback,
          crosapi::VpnServiceForExtensionAsh::FailureCallback>
AdaptCallback(SuccessOrFailureCallback callback) {
  auto [success, failure] = base::SplitOnceCallback(std::move(callback));

  return {base::BindOnce(&RunSuccessCallback, std::move(success)),
          base::BindOnce(&RunFailureCallback, std::move(failure))};
}

}  // namespace

namespace crosapi {

VpnServiceForExtensionAsh::VpnServiceForExtensionAsh(
    const std::string& extension_id,
    chromeos::VpnService* controller)
    : extension_id_(extension_id), controller_(controller) {
  network_configuration_observer_.Observe(
      ash::NetworkHandler::Get()->network_configuration_handler());
}

VpnServiceForExtensionAsh::~VpnServiceForExtensionAsh() = default;

void VpnServiceForExtensionAsh::BindReceiverAndObserver(
    mojo::PendingReceiver<crosapi::mojom::VpnServiceForExtension> receiver,
    mojo::PendingRemote<crosapi::mojom::EventObserverForExtension> observer) {
  receivers_.Add(this, std::move(receiver));
  observers_.Add(std::move(observer));
}

void VpnServiceForExtensionAsh::DestroyConfiguration(
    const std::string& configuration_name,
    DestroyConfigurationCallback callback) {
  const std::string key = GetKey(extension_id(), configuration_name);

  VpnConfiguration* configuration =
      base::FindPtrOrNull(controller_->key_to_configuration_map_, key);
  if (!configuration) {
    RunFailureCallback(std::move(callback), /*error_name=*/{},
                       "Unauthorized access.");
    return;
  }

  // Avoid const ref here since configuration gets removed before service_path
  // is used.
  const std::optional<std::string> service_path = configuration->service_path();

  if (!service_path) {
    RunFailureCallback(std::move(callback), /*error_name=*/{},
                       "Pending create.");
    return;
  }

  if (active_configuration_ == configuration) {
    configuration->OnPlatformMessage(
        base::to_underlying(api_vpn::PlatformMessage::kDisconnected));
  }

  DestroyConfigurationInternal(configuration);

  auto [success, failure] = AdaptCallback(std::move(callback));
  ash::NetworkHandler::Get()
      ->network_configuration_handler()
      ->RemoveConfiguration(
          *service_path,
          /*remove_confirmer=*/{},
          base::BindOnce(
              &VpnServiceForExtensionAsh::OnRemoveConfigurationSuccess,
              weak_factory_.GetWeakPtr(), std::move(success)),
          base::BindOnce(
              &VpnServiceForExtensionAsh::OnRemoveConfigurationFailure,
              weak_factory_.GetWeakPtr(), std::move(failure)));
}

void VpnServiceForExtensionAsh::OnConfigurationRemoved(
    const std::string& service_path,
    const std::string& guid) {
  VpnConfiguration* configuration =
      base::FindPtrOrNull(service_path_to_configuration_map_, service_path);
  if (!configuration) {
    // Ignore removal of a configuration unknown to VPN service, which means
    // the configuration was created internally by the platform or already
    // removed by the extension.
    return;
  }

  DispatchConfigRemovedEvent(configuration->configuration_name());
  DestroyConfigurationInternal(configuration);
}

std::optional<std::string>
VpnServiceForExtensionAsh::GetActiveConfigurationObjectPath() const {
  if (active_configuration_) {
    return active_configuration_->object_path();
  }
  return std::nullopt;
}

bool VpnServiceForExtensionAsh::HasConfigurationForServicePath(
    const std::string& service_path) const {
  return base::Contains(service_path_to_configuration_map_, service_path);
}

void VpnServiceForExtensionAsh::DestroyAllConfigurations() {
  std::vector<std::string> to_be_destroyed;
  for (const auto& [key, configuration] :
       controller_->key_to_configuration_map_) {
    if (configuration->extension_id() == extension_id()) {
      to_be_destroyed.push_back(configuration->configuration_name());
    }
  }
  for (const auto& configuration_name : to_be_destroyed) {
    DestroyConfiguration(configuration_name, base::DoNothing());
  }
}

void VpnServiceForExtensionAsh::CreateConfigurationWithServicePath(
    const std::string& configuration_name,
    const std::string& service_path) {
  DCHECK(!HasConfigurationForServicePath(service_path));
  VpnConfiguration* configuration = controller_->CreateConfigurationInternal(
      extension_id(), configuration_name);
  configuration->set_service_path(service_path);
  service_path_to_configuration_map_[service_path] = configuration;
  ash::ShillThirdPartyVpnDriverClient::Get()->AddShillThirdPartyVpnObserver(
      configuration->object_path(), configuration);
}

void VpnServiceForExtensionAsh::DispatchConfigRemovedEvent(
    const std::string& configuration_name) {
  for (auto& observer : observers_) {
    observer->OnConfigRemoved(configuration_name);
  }
}

void VpnServiceForExtensionAsh::DispatchOnPacketReceivedEvent(
    const std::vector<char>& data) {
  std::vector<uint8_t> data_(data.begin(), data.end());
  for (auto& observer : observers_) {
    observer->OnPacketReceived(data_);
  }
}

void VpnServiceForExtensionAsh::DispatchOnPlatformMessageEvent(
    const std::string& configuration_name,
    int32_t platform_message) {
  for (auto& observer : observers_) {
    observer->OnPlatformMessage(configuration_name, platform_message);
  }
}

// static
std::string VpnServiceForExtensionAsh::GetKey(
    const std::string& extension_id,
    const std::string& configuration_name) {
  const std::string key =
      crypto::SHA256HashString(extension_id + configuration_name);
  return base::HexEncode(key);
}

void VpnServiceForExtensionAsh::DestroyConfigurationInternal(
    VpnConfiguration* configuration) {
  // |owned_configuration| ensures that |configuration| stays valid until the
  // end of the scope.
  auto owned_configuration =
      std::move(controller_->key_to_configuration_map_[configuration->key()]);
  controller_->key_to_configuration_map_.erase(configuration->key());
  if (active_configuration_ == configuration) {
    SetActiveConfiguration(nullptr);
  }

  if (const std::optional<std::string>& service_path =
          configuration->service_path()) {
    ash::ShillThirdPartyVpnDriverClient::Get()
        ->RemoveShillThirdPartyVpnObserver(configuration->object_path());
    service_path_to_configuration_map_.erase(*service_path);
  }
}

void VpnServiceForExtensionAsh::OnCreateConfigurationSuccess(
    SuccessCallback callback,
    VpnConfiguration* configuration,
    const std::string& service_path,
    const std::string& guid) {
  configuration->set_service_path(service_path);
  service_path_to_configuration_map_[service_path] = configuration;
  ash::ShillThirdPartyVpnDriverClient::Get()->AddShillThirdPartyVpnObserver(
      configuration->object_path(), configuration);
  std::move(callback).Run();
}

void VpnServiceForExtensionAsh::OnCreateConfigurationFailure(
    FailureCallback callback,
    VpnConfiguration* configuration,
    const std::string& error_name) {
  DestroyConfigurationInternal(configuration);
  std::move(callback).Run(error_name, /*error_message=*/{});
}

void VpnServiceForExtensionAsh::OnRemoveConfigurationSuccess(
    SuccessCallback callback) {
  std::move(callback).Run();
}

void VpnServiceForExtensionAsh::OnRemoveConfigurationFailure(
    FailureCallback callback,
    const std::string& error_name) {
  std::move(callback).Run(error_name, /*error_message=*/{});
}

void VpnServiceForExtensionAsh::SetActiveConfiguration(
    VpnConfiguration* configuration) {
  active_configuration_ = configuration;
}

VpnServiceAsh::VpnServiceAsh() = default;

VpnServiceAsh::~VpnServiceAsh() = default;

void VpnServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::VpnService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void VpnServiceAsh::RegisterVpnServiceForExtension(
    const std::string& extension_id,
    mojo::PendingReceiver<crosapi::mojom::VpnServiceForExtension> receiver,
    mojo::PendingRemote<crosapi::mojom::EventObserverForExtension> observer) {
  auto* service = GetVpnServiceForExtension(extension_id);
  service->BindReceiverAndObserver(std::move(receiver), std::move(observer));
}

void VpnServiceAsh::MaybeFailActiveConnectionAndDestroyConfigurations(
    const std::string& extension_id,
    bool destroy_configurations) {
  VpnServiceForExtensionAsh* service =
      base::FindPtrOrNull(extension_id_to_service_, extension_id);
  if (!service) {
    return;
  }
  if (std::optional<std::string> object_path =
          service->GetActiveConfigurationObjectPath()) {
    ash::ShillThirdPartyVpnDriverClient::Get()->UpdateConnectionState(
        *object_path,
        base::to_underlying(api_vpn::VpnConnectionState::kFailure),
        base::DoNothing(), base::DoNothing());
  }

  if (destroy_configurations) {
    service->DestroyAllConfigurations();
  }
}

VpnServiceForExtensionAsh* VpnServiceAsh::GetVpnServiceForExtension(
    const std::string& extension_id) {
  auto& service = extension_id_to_service_[extension_id];
  if (!service) {
    service =
        std::make_unique<VpnServiceForExtensionAsh>(extension_id, controller_);
  }
  return service.get();
}

void VpnServiceAsh::Reset() {
  controller_ = nullptr;
  extension_id_to_service_.clear();
}

}  // namespace crosapi
