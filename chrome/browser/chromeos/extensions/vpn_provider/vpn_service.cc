// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service.h"

#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/vpn_service_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/vpn_provider.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_driver_client.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_observer.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "content/public/browser/browser_context.h"
#include "crypto/sha2.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace chromeos {

namespace {

namespace api_vpn = extensions::api::vpn_provider;

// All events that our EventRouter::Observer should be listening to.
// api_vpn::OnConfigCreated is intentionally omitted -- it was never
// implemented.
const char* const kEventNames[] = {
    api_vpn::OnUIEvent::kEventName,
    api_vpn::OnConfigRemoved::kEventName,
    api_vpn::OnPlatformMessage::kEventName,
    api_vpn::OnPacketReceived::kEventName,
};

void RunFailureCallback(chromeos::VpnService::FailureCallback failure,
                        const std::optional<std::string>& error_name,
                        const std::optional<std::string>& error_message) {
  std::move(failure).Run(error_name.value_or(std::string{}),
                         error_message.value_or(std::string{}));
}

bool IsVpnProvider(const extensions::Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kVpnProvider);
}

// Creates a key for VpnService's |key_to_configuration_map_| as a hash of
// |extension_id| and |configuration_name|.
std::string GetKey(const std::string& extension_id,
                   const std::string& configuration_name) {
  const std::string key =
      crypto::SHA256HashString(extension_id + configuration_name);
  return base::HexEncode(key);
}

}  // namespace

class VpnService::VpnConfiguration
    : public crosapi::VpnServiceForExtensionAsh::VpnConfiguration {
 public:
  VpnConfiguration(const std::string& extension_id,
                   const std::string& configuration_name,
                   const std::string& key,
                   VpnService* vpn_service)
      : extension_id_(extension_id),
        configuration_name_(configuration_name),
        key_(key),
        object_path_(shill::kObjectPathBase + key),
        vpn_service_(std::move(vpn_service)) {}

  // VpnServiceAsh::VpnConfiguration:
  const std::string& extension_id() const override { return extension_id_; }
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

  // ash::ShillThirdPartyVpnObserver:
  void OnPacketReceived(const std::vector<char>& data) override;
  void OnPlatformMessage(uint32_t platform_message) override;

 private:
  const std::string extension_id_;
  const std::string configuration_name_;
  const std::string key_;
  const std::string object_path_;
  std::optional<std::string> service_path_;

  // |this| is owned by VpnService.
  raw_ptr<VpnService> vpn_service_ = nullptr;
};

void VpnService::VpnConfiguration::OnPacketReceived(
    const std::vector<char>& data) {
  DCHECK(vpn_service_);
  vpn_service_->GetVpnService()
      ->GetVpnServiceForExtension(extension_id())
      ->DispatchOnPacketReceivedEvent(data);
}

void VpnService::VpnConfiguration::OnPlatformMessage(
    uint32_t platform_message) {
  DCHECK(vpn_service_);
  DCHECK_GE(static_cast<uint32_t>(api_vpn::PlatformMessage::kMaxValue),
            platform_message);

  if (platform_message ==
      std::to_underlying(api_vpn::PlatformMessage::kConnected)) {
    vpn_service_->GetVpnService()->GetVpnServiceForExtension(extension_id());
    vpn_service_->SetActiveConfiguration(this);
  } else if (platform_message ==
                 std::to_underlying(api_vpn::PlatformMessage::kDisconnected) ||
             platform_message ==
                 std::to_underlying(api_vpn::PlatformMessage::kError)) {
    vpn_service_->GetVpnService()->GetVpnServiceForExtension(extension_id());
    vpn_service_->SetActiveConfiguration(nullptr);
  }

  vpn_service_->GetVpnService()
      ->GetVpnServiceForExtension(extension_id())
      ->DispatchOnPlatformMessageEvent(configuration_name(), platform_message);
}

VpnServiceForExtension::VpnServiceForExtension(
    const std::string& extension_id,
    content::BrowserContext* browser_context)
    : extension_id_(extension_id), browser_context_(browser_context) {
  auto* service_remote = VpnService::GetVpnService();
  CHECK(service_remote);
  service_remote->RegisterVpnServiceForExtension(
      extension_id, vpn_service_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote());
}

VpnServiceForExtension::~VpnServiceForExtension() = default;

void VpnServiceForExtension::OnConfigRemoved(
    const std::string& configuration_name) {
  DispatchEvent(std::make_unique<extensions::Event>(
      extensions::events::HistogramValue::VPN_PROVIDER_ON_CONFIG_REMOVED,
      api_vpn::OnConfigRemoved::kEventName,
      api_vpn::OnConfigRemoved::Create(configuration_name), browser_context_));
}

void VpnServiceForExtension::OnPlatformMessage(
    const std::string& configuration_name,
    int32_t platform_message) {
  DispatchEvent(std::make_unique<extensions::Event>(
      extensions::events::VPN_PROVIDER_ON_PLATFORM_MESSAGE,
      api_vpn::OnPlatformMessage::kEventName,
      api_vpn::OnPlatformMessage::Create(
          configuration_name,
          static_cast<api_vpn::PlatformMessage>(platform_message),
          /*error=*/std::string{}),
      browser_context_));
}

void VpnServiceForExtension::OnPacketReceived(
    const std::vector<uint8_t>& data) {
  DispatchEvent(std::make_unique<extensions::Event>(
      extensions::events::VPN_PROVIDER_ON_PACKET_RECEIVED,
      api_vpn::OnPacketReceived::kEventName,
      api_vpn::OnPacketReceived::Create(
          std::vector<uint8_t>(data.begin(), data.end())),
      browser_context_));
}

// TODO(neis): Remove this in favor of VpnService::SendToExtension.
void VpnServiceForExtension::DispatchEvent(
    std::unique_ptr<extensions::Event> event) const {
  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id_, std::move(event));
}

VpnService::VpnService(content::BrowserContext* browser_context)
    : browser_context_(browser_context), vpn_providers_observer_(this) {
  GetVpnService()->SetController(this);

  auto* registry = extensions::ExtensionRegistry::Get(browser_context);
  extension_registry_observer_.Observe(registry);

  auto* event_router = extensions::EventRouter::Get(browser_context);
  for (const char* event_name : kEventNames) {
    event_router->RegisterObserver(this, event_name);
  }

  network_state_handler_observer_.Observe(
      ash::NetworkHandler::Get()->network_state_handler());

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&VpnService::NetworkListChanged,
                                weak_factory_.GetWeakPtr()));
}

VpnService::~VpnService() {
  key_to_configuration_map_.clear();
  GetVpnService()->Reset();
}

void VpnService::SendShowAddDialogToExtension(const std::string& extension_id) {
  SendToExtension(
      extension_id,
      std::make_unique<extensions::Event>(
          extensions::events::HistogramValue::VPN_PROVIDER_ON_UI_EVENT,
          api_vpn::OnUIEvent::kEventName,
          api_vpn::OnUIEvent::Create(api_vpn::UIEvent::kShowAddDialog,
                                     std::string()),
          browser_context_));
}

void VpnService::SendShowConfigureDialogToExtension(
    const std::string& extension_id,
    const std::string& configuration_name) {
  SendToExtension(
      extension_id,
      std::make_unique<extensions::Event>(
          extensions::events::HistogramValue::VPN_PROVIDER_ON_UI_EVENT,
          api_vpn::OnUIEvent::kEventName,
          api_vpn::OnUIEvent::Create(api_vpn::UIEvent::kShowConfigureDialog,
                                     configuration_name),
          browser_context_));
}

void VpnService::SendToExtension(const std::string& extension_id,
                                 std::unique_ptr<extensions::Event> event) {
  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

bool VpnService::OwnsActiveConfiguration(
    const std::string& extension_id) const {
  return GetActiveConfigurationObjectPath(extension_id).has_value();
}

std::optional<std::string> VpnService::GetActiveConfigurationObjectPath(
    const std::string& extension_id) const {
  // Peek into VpnServiceAsh directly (this call does not go through mojo).
  return GetVpnService()
      ->GetVpnServiceForExtension(extension_id)
      ->GetActiveConfigurationObjectPath();
}

void VpnService::SendOnPlatformMessageToExtension(
    const std::string& extension_id,
    const std::string& configuration_name,
    uint32_t platform_message) {
  auto it = extension_id_to_service_.find(extension_id);
  CHECK(it != extension_id_to_service_.end());
  CHECK(it->second);
  it->second->OnPlatformMessage(configuration_name, platform_message);
}

crosapi::VpnServiceForExtensionAsh::VpnConfiguration*
VpnService::LookupConfiguration(const std::string& service_path) {
  return base::FindPtrOrNull(service_path_to_configuration_map_, service_path);
}

crosapi::VpnServiceForExtensionAsh::VpnConfiguration*
VpnService::LookupConfiguration(const std::string& extension_id,
                                const std::string& configuration_name) {
  const std::string key = GetKey(extension_id, configuration_name);
  return base::FindPtrOrNull(key_to_configuration_map_, key);
}

void VpnService::CreateConfiguration(const std::string& extension_id,
                                     const std::string& configuration_name,
                                     SuccessCallback success,
                                     FailureCallback failure) {
  if (configuration_name.empty()) {
    RunFailureCallback(std::move(failure), /*error_name=*/{},
                       "Empty name not supported.");
    return;
  }

  if (LookupConfiguration(extension_id, configuration_name)) {
    RunFailureCallback(std::move(failure), /*error_name=*/{},
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
    RunFailureCallback(std::move(failure), /*error_name=*/{},
                       "No user profile for unshared network configuration.");
    return;
  }

  crosapi::VpnServiceForExtensionAsh::VpnConfiguration* configuration =
      CreateConfigurationInternal(extension_id, configuration_name);

  auto properties =
      base::Value::Dict()
          .Set(shill::kTypeProperty, shill::kTypeVPN)
          .Set(shill::kNameProperty, configuration_name)
          .Set(shill::kProviderHostProperty, extension_id)
          .Set(shill::kObjectPathSuffixProperty,
               GetKey(extension_id, configuration_name))
          .Set(shill::kProviderTypeProperty, shill::kProviderThirdPartyVpn)
          .Set(shill::kProfileProperty, profile->path)
          .Set(shill::kGuidProperty,
               base::Uuid::GenerateRandomV4().AsLowercaseString());

  ash::NetworkHandler::Get()
      ->network_configuration_handler()
      ->CreateShillConfiguration(
          std::move(properties),
          base::BindOnce(&VpnService::OnCreateConfigurationSuccess,
                         weak_factory_.GetWeakPtr(), std::move(success),
                         configuration),
          base::BindOnce(&VpnService::OnCreateConfigurationFailure,
                         weak_factory_.GetWeakPtr(), std::move(failure),
                         configuration));
}

void VpnService::NetworkListChanged() {
  auto* network_handler = ash::NetworkHandler::Get();

  ash::NetworkStateHandler::NetworkStateList network_list;
  network_handler->network_state_handler()->GetVisibleNetworkListByType(
      ash::NetworkTypePattern::VPN(), &network_list);

  for (auto* network_state : network_list) {
    network_handler->network_configuration_handler()->GetShillProperties(
        network_state->path(), base::BindOnce(&VpnService::OnGetShillProperties,
                                              weak_factory_.GetWeakPtr()));
  }
}

void VpnService::OnVpnExtensionsChanged(
    base::flat_set<std::string> vpn_extensions) {
  // No changes to the existing set?
  if (vpn_extensions_ == vpn_extensions) {
    return;
  }
  vpn_extensions_ = std::move(vpn_extensions);
  NetworkListChanged();
}

void VpnService::OnGetShillProperties(
    const std::string& service_path,
    std::optional<base::Value::Dict> configuration_properties) {
  if (!configuration_properties || LookupConfiguration(service_path)) {
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

  crosapi::VpnServiceForExtensionAsh::VpnConfiguration* configuration =
      CreateConfigurationInternal(*extension_id, *configuration_name);
  RegisterConfiguration(configuration, service_path);
}

void VpnService::DestroyConfiguration(const std::string& extension_id,
                                      const std::string& configuration_name,
                                      SuccessCallback success,
                                      FailureCallback failure) {
  crosapi::VpnServiceForExtensionAsh::VpnConfiguration* configuration =
      LookupConfiguration(extension_id, configuration_name);
  if (!configuration) {
    RunFailureCallback(std::move(failure), /*error_name=*/{},
                       "Unauthorized access.");
    return;
  }

  // Avoid const ref here since configuration gets removed before service_path
  // is used.
  const std::optional<std::string> service_path = configuration->service_path();
  if (!service_path) {
    RunFailureCallback(std::move(failure), /*error_name=*/{},
                       "Pending create.");
    return;
  }

  if (active_configuration_ == configuration) {
    configuration->OnPlatformMessage(
        std::to_underlying(api_vpn::PlatformMessage::kDisconnected));
  }

  DestroyConfigurationInternal(configuration);

  ash::NetworkHandler::Get()
      ->network_configuration_handler()
      ->RemoveConfiguration(
          *service_path,
          /*remove_confirmer=*/{},
          base::BindOnce(&VpnService::OnRemoveConfigurationSuccess,
                         weak_factory_.GetWeakPtr(), std::move(success)),
          base::BindOnce(&VpnService::OnRemoveConfigurationFailure,
                         weak_factory_.GetWeakPtr(), std::move(failure)));
}

void VpnService::DestroyConfigurationInternal(
    crosapi::VpnServiceForExtensionAsh::VpnConfiguration* configuration) {
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

void VpnService::OnRemoveConfigurationSuccess(SuccessCallback callback) {
  std::move(callback).Run();
}

void VpnService::OnRemoveConfigurationFailure(FailureCallback callback,
                                              const std::string& error_name) {
  std::move(callback).Run(error_name, /*error_message=*/{});
}

void VpnService::SetActiveConfiguration(
    crosapi::VpnServiceForExtensionAsh::VpnConfiguration* configuration) {
  active_configuration_ = configuration;
}

void VpnService::SetParameters(const std::string& extension_id,
                               base::Value::Dict parameters,
                               SuccessCallback success,
                               FailureCallback failure) {
  if (!OwnsActiveConfiguration(extension_id)) {
    RunFailureCallback(std::move(failure), /*error_name=*/{},
                       "Unauthorized access.");
    return;
  }

  ash::ShillThirdPartyVpnDriverClient::Get()->SetParameters(
      GetActiveConfigurationObjectPath(extension_id).value(),
      std::move(parameters),
      base::IgnoreArgs<const std::string&>(std::move(success)),
      std::move(failure));
}

void VpnService::SendPacket(const std::string& extension_id,
                            const std::vector<char>& data,
                            SuccessCallback success,
                            FailureCallback failure) {
  if (!OwnsActiveConfiguration(extension_id)) {
    RunFailureCallback(std::move(failure), /*error_name=*/{},
                       "Unauthorized access.");
    return;
  }

  if (data.empty()) {
    RunFailureCallback(std::move(failure), /*error_name=*/{},
                       "Can't send an empty packet.");
    return;
  }

  ash::ShillThirdPartyVpnDriverClient::Get()->SendPacket(
      GetActiveConfigurationObjectPath(extension_id).value(), data,
      std::move(success), std::move(failure));
}

void VpnService::NotifyConnectionStateChanged(const std::string& extension_id,
                                              bool connection_success,
                                              SuccessCallback success,
                                              FailureCallback failure) {
  if (!OwnsActiveConfiguration(extension_id)) {
    RunFailureCallback(std::move(failure), /*error_name=*/{},
                       "Unauthorized access.");
    return;
  }

  ash::ShillThirdPartyVpnDriverClient::Get()->UpdateConnectionState(
      GetActiveConfigurationObjectPath(extension_id).value(),
      connection_success
          ? std::to_underlying(
                extensions::api::vpn_provider::VpnConnectionState::kConnected)
          : std::to_underlying(
                extensions::api::vpn_provider::VpnConnectionState::kFailure),
      std::move(success), std::move(failure));
}

void VpnService::Shutdown() {
  extensions::EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void VpnService::OnExtensionUninstalled(content::BrowserContext*,
                                        const extensions::Extension* extension,
                                        extensions::UninstallReason) {
  // Extension should have a vpnProvider permission in order to use the API;
  // therefore we can safely ignore all other extensions (because otherwise
  // we'll just make an unnecessary mojo call).
  if (!IsVpnProvider(extension)) {
    return;
  }
  GetVpnService()->MaybeFailActiveConnectionAndDestroyConfigurations(
      extension->id(), /*destroy_configurations=*/true);
  extension_id_to_service_.erase(extension->id());
}

void VpnService::OnExtensionUnloaded(
    content::BrowserContext*,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  // Extension should have a vpnProvider permission in order to use the API;
  // therefore we can safely ignore all other extensions (because otherwise
  // we'll just make an unnecessary mojo call).
  if (!IsVpnProvider(extension)) {
    return;
  }
  bool destroy_configurations =
      reason == extensions::UnloadedExtensionReason::DISABLE ||
      reason == extensions::UnloadedExtensionReason::BLOCKLIST;
  GetVpnService()->MaybeFailActiveConnectionAndDestroyConfigurations(
      extension->id(), destroy_configurations);
  if (destroy_configurations) {
    extension_id_to_service_.erase(extension->id());
  }
}

crosapi::VpnServiceForExtensionAsh::VpnConfiguration*
VpnService::CreateConfigurationInternal(const std::string& extension_id,
                                        const std::string& configuration_name) {
  const std::string key = GetKey(extension_id, configuration_name);
  auto configuration = std::make_unique<VpnConfiguration>(
      extension_id, configuration_name, key, this);
  auto* ptr = configuration.get();
  key_to_configuration_map_.emplace(key, std::move(configuration));
  return ptr;
}

void VpnService::OnCreateConfigurationSuccess(
    SuccessCallback callback,
    crosapi::VpnServiceForExtensionAsh::VpnConfiguration* configuration,
    const std::string& service_path,
    const std::string& guid) {
  RegisterConfiguration(configuration, service_path);
  std::move(callback).Run();
}

void VpnService::RegisterConfiguration(
    crosapi::VpnServiceForExtensionAsh::VpnConfiguration* configuration,
    const std::string& service_path) {
  // Ensure the corresponding VpnServiceForExtensionAsh is created.
  GetVpnService()->GetVpnServiceForExtension(configuration->extension_id());

  configuration->set_service_path(service_path);
  auto [_, inserted] =
      service_path_to_configuration_map_.emplace(service_path, configuration);
  CHECK(inserted);
  ash::ShillThirdPartyVpnDriverClient::Get()->AddShillThirdPartyVpnObserver(
      configuration->object_path(), configuration);
}

void VpnService::OnCreateConfigurationFailure(
    FailureCallback callback,
    crosapi::VpnServiceForExtensionAsh::VpnConfiguration* configuration,
    const std::string& error_name) {
  DestroyConfigurationInternal(configuration);
  std::move(callback).Run(error_name, /*error_message=*/{});
}

void VpnService::OnListenerAdded(const extensions::EventListenerInfo& details) {
  // Ensures that the service is created for the extension, so that incoming VPN
  // events can be dispatched to the extension.
  GetVpnServiceForExtension(details.extension_id);
}

// static
crosapi::VpnServiceAsh* VpnService::GetVpnService() {
  // CrosapiManager may not be initialized.
  // TODO(crbug.com/40225953): Assert it's only happening in tests.
  if (!crosapi::CrosapiManager::IsInitialized()) {
    LOG(ERROR) << "CrosapiManager is not initialized.";
    return nullptr;
  }
  return crosapi::CrosapiManager::Get()->crosapi_ash()->vpn_service_ash();
}

mojo::Remote<crosapi::mojom::VpnServiceForExtension>&
VpnService::GetVpnServiceForExtension(const std::string& extension_id) {
  auto& service = extension_id_to_service_[extension_id];
  if (!service) {
    service = std::make_unique<VpnServiceForExtension>(extension_id,
                                                       browser_context_);
  }
  return service->Proxy();
}

std::string VpnService::GetKeyForTesting(
    const std::string& extension_id,
    const std::string& configuration_name) {
  return GetKey(extension_id, configuration_name);
}

}  // namespace chromeos
