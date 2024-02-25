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
#include "base/types/cxx23_to_underlying.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
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

void RunWarningCallback(
    crosapi::VpnServiceForExtensionAsh::SuccessCallback callback,
    const std::string& /*warning*/) {
  std::move(callback).Run();
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

VpnProvidersObserver::VpnProvidersObserver(
    VpnProvidersObserver::Delegate* delegate)
    : delegate_(delegate) {
  ash::GetNetworkConfigService(
      cros_network_config_.BindNewPipeAndPassReceiver());
  cros_network_config_->AddObserver(
      cros_network_config_observer_.BindNewPipeAndPassRemote());
}

VpnProvidersObserver::~VpnProvidersObserver() = default;

void VpnProvidersObserver::OnVpnProvidersChanged() {
  cros_network_config_->GetVpnProviders(base::BindOnce(
      &VpnProvidersObserver::OnGetVpnProviders, weak_factory_.GetWeakPtr()));
}

void VpnProvidersObserver::OnGetVpnProviders(
    std::vector<chromeos::network_config::mojom::VpnProviderPtr>
        vpn_providers) {
  if (!delegate_) {
    return;
  }
  base::flat_set<std::string> vpn_extensions;
  for (const auto& vpn_provider : vpn_providers) {
    if (vpn_provider->type ==
        chromeos::network_config::mojom::VpnType::kExtension) {
      vpn_extensions.insert(vpn_provider->app_id);
    }
  }
  delegate_->OnVpnExtensionsChanged(std::move(vpn_extensions));
}

class VpnConfigurationImpl
    : public VpnServiceForExtensionAsh::VpnConfiguration {
 public:
  VpnConfigurationImpl(const std::string& configuration_name,
                       const std::string& key,
                       VpnServiceForExtensionAsh* vpn_service)
      : configuration_name_(configuration_name),
        key_(key),
        object_path_(shill::kObjectPathBase + key),
        vpn_service_(std::move(vpn_service)) {}

  // VpnServiceAsh::VpnConfiguration:
  const std::string& configuration_name() const override {
    return configuration_name_;
  }
  const std::string& key() const override { return key_; }
  const std::string& object_path() const override { return object_path_; }
  const std::optional<std::string>& service_path() const override {
    return service_path_;
  }
  void set_service_path(std::string service_path) override {
    service_path_ = std::move(service_path);
  }

  void BindPepperVpnProxyObserver(
      mojo::PendingRemote<crosapi::mojom::PepperVpnProxyObserver>
          pepper_vpn_proxy_observer) override {
    pepper_vpn_proxy_observer_.Bind(std::move(pepper_vpn_proxy_observer));
  }

  // ash::ShillThirdPartyVpnObserver:
  void OnPacketReceived(const std::vector<char>& data) override;
  void OnPlatformMessage(uint32_t platform_message) override;

 private:
  const std::string configuration_name_;
  const std::string key_;
  const std::string object_path_;
  std::optional<std::string> service_path_;

  mojo::Remote<crosapi::mojom::PepperVpnProxyObserver>
      pepper_vpn_proxy_observer_;

  // |this| is owned by VpnServiceForExtensionAsh.
  raw_ptr<VpnServiceForExtensionAsh> vpn_service_ = nullptr;
};

void VpnConfigurationImpl::OnPacketReceived(const std::vector<char>& data) {
  DCHECK(vpn_service_);
  // If Pepper observer is bound, route the packet through the Pepper API.
  if (pepper_vpn_proxy_observer_) {
    pepper_vpn_proxy_observer_->OnPacketReceived(
        std::vector<uint8_t>(data.begin(), data.end()));
  } else {
    vpn_service_->DispatchOnPacketReceivedEvent(data);
  }
}

void VpnConfigurationImpl::OnPlatformMessage(uint32_t platform_message) {
  DCHECK(vpn_service_);
  DCHECK_GE(static_cast<uint32_t>(api_vpn::PlatformMessage::kMaxValue),
            platform_message);

  if (platform_message ==
      base::to_underlying(api_vpn::PlatformMessage::kConnected)) {
    vpn_service_->SetActiveConfiguration(this);
  } else if (platform_message ==
                 base::to_underlying(api_vpn::PlatformMessage::kDisconnected) ||
             platform_message ==
                 base::to_underlying(api_vpn::PlatformMessage::kError)) {
    vpn_service_->SetActiveConfiguration(nullptr);
    if (pepper_vpn_proxy_observer_) {
      pepper_vpn_proxy_observer_->OnUnbind();
      pepper_vpn_proxy_observer_.reset();
    }
  }

  vpn_service_->DispatchOnPlatformMessageEvent(configuration_name(),
                                               platform_message);
}

VpnServiceForExtensionAsh::VpnServiceForExtensionAsh(
    const std::string& extension_id)
    : extension_id_(extension_id) {
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

void VpnServiceForExtensionAsh::CreateConfiguration(
    const std::string& configuration_name,
    CreateConfigurationCallback callback) {
  if (configuration_name.empty()) {
    RunFailureCallback(std::move(callback), /*error_name=*/{},
                       "Empty name not supported.");
    return;
  }

  const std::string key = GetKey(extension_id(), configuration_name);
  if (base::Contains(key_to_configuration_map_, key)) {
    RunFailureCallback(std::move(callback), /*error_name=*/{},
                       "Name not unique.");
    return;
  }

  // Since the API is only designed to be used with the primary profile, it's
  // safe to get the hash of the primary profile here.
  const ash::NetworkProfile* profile =
      ash::NetworkHandler::Get()
          ->network_profile_handler()
          ->GetProfileForUserhash(ash::ProfileHelper::GetUserIdHashFromProfile(
              ProfileManager::GetPrimaryUserProfile()));
  if (!profile) {
    RunFailureCallback(std::move(callback), /*error_name=*/{},
                       "No user profile for unshared network configuration.");
    return;
  }

  VpnConfiguration* configuration =
      CreateConfigurationInternal(configuration_name);

  auto properties =
      base::Value::Dict()
          .Set(shill::kTypeProperty, shill::kTypeVPN)
          .Set(shill::kNameProperty, configuration_name)
          .Set(shill::kProviderHostProperty, extension_id())
          .Set(shill::kObjectPathSuffixProperty, key)
          .Set(shill::kProviderTypeProperty, shill::kProviderThirdPartyVpn)
          .Set(shill::kProfileProperty, profile->path)
          .Set(shill::kGuidProperty,
               base::Uuid::GenerateRandomV4().AsLowercaseString());

  auto [success, failure] = AdaptCallback(std::move(callback));
  ash::NetworkHandler::Get()
      ->network_configuration_handler()
      ->CreateShillConfiguration(
          std::move(properties),
          base::BindOnce(
              &VpnServiceForExtensionAsh::OnCreateConfigurationSuccess,
              weak_factory_.GetWeakPtr(), std::move(success), configuration),
          base::BindOnce(
              &VpnServiceForExtensionAsh::OnCreateConfigurationFailure,
              weak_factory_.GetWeakPtr(), std::move(failure), configuration));
}

void VpnServiceForExtensionAsh::DestroyConfiguration(
    const std::string& configuration_name,
    DestroyConfigurationCallback callback) {
  const std::string key = GetKey(extension_id(), configuration_name);

  VpnConfiguration* configuration =
      base::FindPtrOrNull(key_to_configuration_map_, key);
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

void VpnServiceForExtensionAsh::SetParameters(base::Value::Dict parameters,
                                              SetParametersCallback callback) {
  if (!OwnsActiveConfiguration()) {
    RunFailureCallback(std::move(callback), /*error_name=*/{},
                       "Unauthorized access.");
    return;
  }

  auto [success, failure] = AdaptCallback(std::move(callback));
  ash::ShillThirdPartyVpnDriverClient::Get()->SetParameters(
      active_configuration_->object_path(), std::move(parameters),
      base::BindOnce(&RunWarningCallback, std::move(success)),
      std::move(failure));
}

void VpnServiceForExtensionAsh::SendPacket(const std::vector<uint8_t>& data,
                                           SendPacketCallback callback) {
  if (!OwnsActiveConfiguration()) {
    RunFailureCallback(std::move(callback), /*error_name=*/{},
                       "Unauthorized access.");
    return;
  }

  if (data.empty()) {
    RunFailureCallback(std::move(callback), /*error_name=*/{},
                       "Can't send an empty packet.");
    return;
  }

  auto [success, failure] = AdaptCallback(std::move(callback));
  ash::ShillThirdPartyVpnDriverClient::Get()->SendPacket(
      active_configuration_->object_path(),
      std::vector<char>(data.begin(), data.end()), std::move(success),
      std::move(failure));
}

void VpnServiceForExtensionAsh::NotifyConnectionStateChanged(
    bool connection_success,
    NotifyConnectionStateChangedCallback callback) {
  if (!OwnsActiveConfiguration()) {
    RunFailureCallback(std::move(callback), /*error_name=*/{},
                       "Unauthorized access.");
    return;
  }

  auto [success, failure] = AdaptCallback(std::move(callback));
  ash::ShillThirdPartyVpnDriverClient::Get()->UpdateConnectionState(
      active_configuration_->object_path(),
      connection_success
          ? base::to_underlying(api_vpn::VpnConnectionState::kConnected)
          : base::to_underlying(api_vpn::VpnConnectionState::kFailure),
      std::move(success), std::move(failure));
}

void VpnServiceForExtensionAsh::BindPepperVpnProxyObserver(
    const std::string& configuration_name,
    mojo::PendingRemote<crosapi::mojom::PepperVpnProxyObserver>
        pepper_vpn_proxy_observer,
    BindPepperVpnProxyObserverCallback callback) {
  const std::string key = GetKey(extension_id(), configuration_name);

  VpnConfiguration* configuration =
      base::FindPtrOrNull(key_to_configuration_map_, key);
  if (!configuration) {
    RunFailureCallback(
        std::move(callback), /*error_name=*/{},
        "Unauthorized access. The configuration does not exist.");
    return;
  }

  if (active_configuration_ != configuration) {
    RunFailureCallback(std::move(callback), /*error_name=*/{},
                       "Unauthorized access. The configuration is not active.");
    return;
  }

  if (configuration->configuration_name() != configuration_name) {
    RunFailureCallback(
        std::move(callback), /*error_name=*/{},
        "Unauthorized access. Configuration name or extension ID mismatch.");
    return;
  }

  if (!configuration->service_path()) {
    RunFailureCallback(std::move(callback), /*error_name=*/{},
                       "Pending create.");
    return;
  }

  // Connection authorized. All packets will be routed through the Pepper API.
  configuration->BindPepperVpnProxyObserver(
      std::move(pepper_vpn_proxy_observer));

  RunSuccessCallback(std::move(callback));
}

void VpnServiceForExtensionAsh::DispatchAddDialogEvent() {
  for (auto& observer : observers_) {
    observer->OnAddDialog();
  }
}

void VpnServiceForExtensionAsh::DispatchConfigureDialogEvent(
    const std::string& configuration_name) {
  for (auto& observer : observers_) {
    observer->OnConfigureDialog(configuration_name);
  }
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

bool VpnServiceForExtensionAsh::OwnsActiveConfiguration() const {
  return !!active_configuration_;
}

bool VpnServiceForExtensionAsh::HasConfigurationForServicePath(
    const std::string& service_path) const {
  return base::Contains(service_path_to_configuration_map_, service_path);
}

void VpnServiceForExtensionAsh::DestroyAllConfigurations() {
  std::vector<std::string> to_be_destroyed;
  for (const auto& [key, configuration] : key_to_configuration_map_) {
    to_be_destroyed.push_back(configuration->configuration_name());
  }
  for (const auto& configuration_name : to_be_destroyed) {
    DestroyConfiguration(configuration_name, base::DoNothing());
  }
}

void VpnServiceForExtensionAsh::CreateConfigurationWithServicePath(
    const std::string& configuration_name,
    const std::string& service_path) {
  DCHECK(!HasConfigurationForServicePath(service_path));
  VpnConfiguration* configuration =
      CreateConfigurationInternal(configuration_name);
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
    int32_t platform_message,
    const std::optional<std::string>& error) {
  for (auto& observer : observers_) {
    observer->OnPlatformMessage(configuration_name, platform_message, error);
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

VpnServiceForExtensionAsh::VpnConfiguration*
VpnServiceForExtensionAsh::CreateConfigurationInternal(
    const std::string& configuration_name) {
  const std::string key = GetKey(extension_id(), configuration_name);
  auto configuration =
      std::make_unique<VpnConfigurationImpl>(configuration_name, key, this);
  auto* ptr = configuration.get();
  key_to_configuration_map_.emplace(key, std::move(configuration));
  return ptr;
}

void VpnServiceForExtensionAsh::DestroyConfigurationInternal(
    VpnConfiguration* configuration) {
  // |owned_configuration| ensures that |configuration| stays valid until the
  // end of the scope.
  auto owned_configuration =
      std::move(key_to_configuration_map_[configuration->key()]);
  key_to_configuration_map_.erase(configuration->key());
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

VpnServiceAsh::VpnServiceAsh() {
  // Can be false in unit tests.
  if (!ash::NetworkHandler::IsInitialized()) {
    return;
  }

  network_state_handler_observer_.Observe(
      ash::NetworkHandler::Get()->network_state_handler());

  vpn_providers_observer_ = std::make_unique<VpnProvidersObserver>(this);
}

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
  service->NotifyConnectionStateChanged(
      /*connection_success=*/false, base::DoNothing());

  if (destroy_configurations) {
    service->DestroyAllConfigurations();
  }
}

void VpnServiceAsh::NetworkListChanged() {
  ash::NetworkStateHandler::NetworkStateList network_list;

  auto* network_handler = ash::NetworkHandler::Get();
  network_handler->network_state_handler()->GetVisibleNetworkListByType(
      ash::NetworkTypePattern::VPN(), &network_list);

  for (auto* network_state : network_list) {
    network_handler->network_configuration_handler()->GetShillProperties(
        network_state->path(),
        base::BindOnce(&VpnServiceAsh::OnGetShillProperties,
                       weak_factory_.GetWeakPtr()));
  }
}

void VpnServiceAsh::OnVpnExtensionsChanged(
    base::flat_set<std::string> vpn_extensions) {
  // No changes to the existing set?
  if (vpn_extensions_ == vpn_extensions) {
    return;
  }
  vpn_extensions_ = std::move(vpn_extensions);
  NetworkListChanged();
}

void VpnServiceAsh::OnGetShillProperties(
    const std::string& service_path,
    std::optional<base::Value::Dict> configuration_properties) {
  if (!configuration_properties) {
    return;
  }
  const std::string* vpn_type =
      configuration_properties->FindStringByDottedPath(
          shill::kProviderTypeProperty);
  const std::string* extension_id =
      configuration_properties->FindStringByDottedPath(
          shill::kProviderHostProperty);
  const std::string* type =
      configuration_properties->FindStringByDottedPath(shill::kTypeProperty);
  const std::string* configuration_name =
      configuration_properties->FindStringByDottedPath(shill::kNameProperty);
  if (!vpn_type || !extension_id || !type || !configuration_name ||
      *vpn_type != shill::kProviderThirdPartyVpn || *type != shill::kTypeVPN) {
    return;
  }

  if (!base::Contains(vpn_extensions_, *extension_id)) {
    return;
  }

  auto* service = GetVpnServiceForExtension(*extension_id);
  if (service->HasConfigurationForServicePath(service_path)) {
    return;
  }
  service->CreateConfigurationWithServicePath(*configuration_name,
                                              service_path);
}

VpnServiceForExtensionAsh* VpnServiceAsh::GetVpnServiceForExtension(
    const std::string& extension_id) {
  auto& service = extension_id_to_service_[extension_id];
  if (!service) {
    service = std::make_unique<VpnServiceForExtensionAsh>(extension_id);
  }
  return service.get();
}

}  // namespace crosapi
