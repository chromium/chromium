// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service.h"

#include <optional>
#include <utility>

#include "base/check_deref.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/vpn_provider.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_driver_client.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_observer.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "content/public/browser/browser_context.h"
#include "crypto/sha2.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"

namespace chromeos {

namespace {

namespace api_vpn = extensions::api::vpn_provider;

// Creates a key for VpnService's |key_to_configuration_map_| as a hash of
// |extension_id| and |configuration_name|.
std::string GetKey(const std::string& extension_id,
                   const std::string& configuration_name) {
  const std::string key =
      crypto::SHA256HashString(extension_id + configuration_name);
  return base::HexEncode(key);
}

}  // namespace

class VpnService::VpnConfiguration : public ash::ShillThirdPartyVpnObserver {
 public:
  VpnConfiguration(const std::string& extension_id,
                   const std::string& configuration_name,
                   const std::string& key,
                   VpnService* vpn_service)
      : extension_id_(extension_id),
        configuration_name_(configuration_name),
        key_(key),
        object_path_(shill::kObjectPathBase + key),
        vpn_service_(CHECK_DEREF(std::move(vpn_service))) {}

  const std::string& extension_id() const { return extension_id_; }
  const std::string& configuration_name() const { return configuration_name_; }
  const std::string& key() const { return key_; }
  const std::string& object_path() const { return object_path_; }
  const std::optional<std::string>& service_path() const {
    return service_path_;
  }
  void set_service_path(std::string service_path) {
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
  const raw_ref<VpnService> vpn_service_;
};

void VpnService::VpnConfiguration::OnPacketReceived(
    const std::vector<char>& data) {
  vpn_service_->SendOnPacketReceivedToExtension(extension_id(), data);
}

void VpnService::VpnConfiguration::OnPlatformMessage(
    uint32_t platform_message) {
  DCHECK_GE(static_cast<uint32_t>(api_vpn::PlatformMessage::kMaxValue),
            platform_message);

  if (platform_message ==
      std::to_underlying(api_vpn::PlatformMessage::kConnected)) {
    vpn_service_->SetActiveConfiguration(this);
  } else if (platform_message ==
                 std::to_underlying(api_vpn::PlatformMessage::kDisconnected) ||
             platform_message ==
                 std::to_underlying(api_vpn::PlatformMessage::kError)) {
    vpn_service_->SetActiveConfiguration(nullptr);
  }

  vpn_service_->SendOnPlatformMessageToExtension(
      extension_id(), configuration_name(), platform_message);
}

VpnService::VpnService(content::BrowserContext* browser_context)
    : browser_context_(CHECK_DEREF(browser_context)) {
  extension_registry_observer_.Observe(
      extensions::ExtensionRegistry::Get(browser_context));
  network_configuration_observer_.Observe(
      ash::NetworkHandler::Get()->network_configuration_handler());
  network_state_handler_observer_.Observe(
      ash::NetworkHandler::Get()->network_state_handler());

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&VpnService::NetworkListChanged,
                                weak_factory_.GetWeakPtr()));
}

VpnService::~VpnService() = default;

void VpnService::SendShowAddDialogToExtension(const std::string& extension_id) {
  SendToExtension(
      extension_id,
      std::make_unique<extensions::Event>(
          extensions::events::HistogramValue::VPN_PROVIDER_ON_UI_EVENT,
          api_vpn::OnUIEvent::kEventName,
          api_vpn::OnUIEvent::Create(api_vpn::UIEvent::kShowAddDialog,
                                     std::string()),
          &browser_context_.get()));
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
          &browser_context_.get()));
}

void VpnService::SendToExtension(const std::string& extension_id,
                                 std::unique_ptr<extensions::Event> event) {
  extensions::EventRouter::Get(&browser_context_.get())
      ->DispatchEventToExtension(extension_id, std::move(event));
}

VpnService::VpnConfiguration* VpnService::GetActiveConfigurationForExtension(
    const std::string& extension_id) const {
  return (active_configuration_ &&
          active_configuration_->extension_id() == extension_id)
             ? active_configuration_
             : nullptr;
}

void VpnService::SendOnPacketReceivedToExtension(
    const std::string& extension_id,
    const std::vector<char>& data) {
  SendToExtension(extension_id,
                  std::make_unique<extensions::Event>(
                      extensions::events::VPN_PROVIDER_ON_PACKET_RECEIVED,
                      api_vpn::OnPacketReceived::kEventName,
                      api_vpn::OnPacketReceived::Create(
                          std::vector<uint8_t>(data.begin(), data.end())),
                      &browser_context_.get()));
}

void VpnService::SendOnPlatformMessageToExtension(
    const std::string& extension_id,
    const std::string& configuration_name,
    uint32_t platform_message) {
  SendToExtension(
      extension_id,
      std::make_unique<extensions::Event>(
          extensions::events::VPN_PROVIDER_ON_PLATFORM_MESSAGE,
          api_vpn::OnPlatformMessage::kEventName,
          api_vpn::OnPlatformMessage::Create(
              configuration_name,
              static_cast<api_vpn::PlatformMessage>(platform_message),
              std::string{}),
          &browser_context_.get()));
}

void VpnService::SendOnConfigRemovedToExtension(
    const std::string& extension_id,
    const std::string& configuration_name) {
  SendToExtension(
      extension_id,
      std::make_unique<extensions::Event>(
          extensions::events::HistogramValue::VPN_PROVIDER_ON_CONFIG_REMOVED,
          api_vpn::OnConfigRemoved::kEventName,
          api_vpn::OnConfigRemoved::Create(configuration_name),
          &browser_context_.get()));
}

VpnService::VpnConfiguration* VpnService::LookupConfiguration(
    const std::string& service_path) {
  return base::FindPtrOrNull(service_path_to_configuration_map_, service_path);
}

VpnService::VpnConfiguration* VpnService::LookupConfiguration(
    const std::string& extension_id,
    const std::string& configuration_name) {
  const std::string key = GetKey(extension_id, configuration_name);
  return base::FindPtrOrNull(key_to_configuration_map_, key);
}

void VpnService::OnConfigurationRemoved(const std::string& service_path,
                                        const std::string& /*guid*/) {
  VpnConfiguration* configuration = LookupConfiguration(service_path);
  if (!configuration) {
    // Ignore removal of a configuration unknown to VPN service, which means
    // the configuration was created internally by the platform or already
    // removed by the extension.
    return;
  }

  SendOnConfigRemovedToExtension(configuration->extension_id(),
                                 configuration->configuration_name());
  DestroyConfigurationInternal(configuration);
}

void VpnService::CreateConfiguration(const std::string& extension_id,
                                     const std::string& configuration_name,
                                     SuccessCallback success,
                                     FailureCallback failure) {
  if (configuration_name.empty()) {
    std::move(failure).Run(/*error_name=*/"", "Empty name not supported.");
    return;
  }

  if (LookupConfiguration(extension_id, configuration_name)) {
    std::move(failure).Run(/*error_name=*/"", "Name not unique.");
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
    std::move(failure).Run(
        /*error_name=*/"",
        "No user profile for unshared network configuration.");
    return;
  }

  VpnService::VpnConfiguration* configuration =
      CreateConfigurationInternal(extension_id, configuration_name);

  auto properties =
      base::DictValue()
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

void VpnService::OnGetShillProperties(
    const std::string& service_path,
    std::optional<base::DictValue> configuration_properties) {
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

  if (!extensions::ExtensionRegistry::Get(&browser_context_.get())
           ->GetExtensionById(*extension_id,
                              extensions::ExtensionRegistry::ENABLED)) {
    // Does not belong to this instance of VpnService.
    return;
  }

  VpnService::VpnConfiguration* configuration =
      CreateConfigurationInternal(*extension_id, *configuration_name);
  RegisterConfiguration(configuration, service_path);
}

void VpnService::DestroyConfiguration(const std::string& extension_id,
                                      const std::string& configuration_name,
                                      SuccessCallback success,
                                      FailureCallback failure) {
  VpnService::VpnConfiguration* configuration =
      LookupConfiguration(extension_id, configuration_name);
  if (!configuration) {
    std::move(failure).Run(/*error_name=*/"", "Unauthorized access.");
    return;
  }

  // Avoid const ref here since configuration gets removed before service_path
  // is used.
  const std::optional<std::string> service_path = configuration->service_path();
  if (!service_path) {
    std::move(failure).Run(/*error_name=*/"", "Pending create.");
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
    VpnService::VpnConfiguration* configuration) {
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
    VpnService::VpnConfiguration* configuration) {
  active_configuration_ = configuration;
}

void VpnService::SetParameters(const std::string& extension_id,
                               base::DictValue parameters,
                               SuccessCallback success,
                               FailureCallback failure) {
  VpnConfiguration* active_configuration =
      GetActiveConfigurationForExtension(extension_id);
  if (!active_configuration) {
    std::move(failure).Run(/*error_name=*/"", "Unauthorized access.");
    return;
  }

  ash::ShillThirdPartyVpnDriverClient::Get()->SetParameters(
      active_configuration->object_path(), std::move(parameters),
      base::IgnoreArgs<const std::string&>(std::move(success)),
      std::move(failure));
}

void VpnService::SendPacket(const std::string& extension_id,
                            const std::vector<char>& data,
                            SuccessCallback success,
                            FailureCallback failure) {
  VpnConfiguration* active_configuration =
      GetActiveConfigurationForExtension(extension_id);
  if (!active_configuration) {
    std::move(failure).Run(/*error_name=*/"", "Unauthorized access.");
    return;
  }

  if (data.empty()) {
    std::move(failure).Run(/*error_name=*/"", "Can't send an empty packet.");
    return;
  }

  ash::ShillThirdPartyVpnDriverClient::Get()->SendPacket(
      active_configuration->object_path(), data, std::move(success),
      std::move(failure));
}

void VpnService::NotifyConnectionStateChanged(const std::string& extension_id,
                                              bool connection_success,
                                              SuccessCallback success,
                                              FailureCallback failure) {
  VpnConfiguration* active_configuration =
      GetActiveConfigurationForExtension(extension_id);
  if (!active_configuration) {
    std::move(failure).Run(/*error_name=*/"", "Unauthorized access.");
    return;
  }

  ash::ShillThirdPartyVpnDriverClient::Get()->UpdateConnectionState(
      active_configuration->object_path(),
      connection_success
          ? std::to_underlying(
                extensions::api::vpn_provider::VpnConnectionState::kConnected)
          : std::to_underlying(
                extensions::api::vpn_provider::VpnConnectionState::kFailure),
      std::move(success), std::move(failure));
}

void VpnService::OnExtensionUninstalled(content::BrowserContext*,
                                        const extensions::Extension* extension,
                                        extensions::UninstallReason) {
  DestroyConfigurationsForExtension(extension->id());
}

void VpnService::OnExtensionUnloaded(
    content::BrowserContext*,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  VpnConfiguration* active_configuration =
      GetActiveConfigurationForExtension(extension->id());
  if (active_configuration) {
    ash::ShillThirdPartyVpnDriverClient::Get()->UpdateConnectionState(
        active_configuration->object_path(),
        std::to_underlying(api_vpn::VpnConnectionState::kFailure),
        base::DoNothing(), base::DoNothing());
  }
  if (reason == extensions::UnloadedExtensionReason::DISABLE ||
      reason == extensions::UnloadedExtensionReason::BLOCKLIST) {
    DestroyConfigurationsForExtension(extension->id());
  }
}

void VpnService::DestroyConfigurationsForExtension(
    const std::string& extension_id) {
  std::vector<std::string> to_be_destroyed;
  for (const auto& [_, configuration] : key_to_configuration_map_) {
    if (configuration->extension_id() == extension_id) {
      to_be_destroyed.push_back(configuration->configuration_name());
    }
  }
  for (const auto& configuration_name : to_be_destroyed) {
    DestroyConfiguration(extension_id, configuration_name, base::DoNothing(),
                         base::DoNothing());
  }
}

VpnService::VpnConfiguration* VpnService::CreateConfigurationInternal(
    const std::string& extension_id,
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
    VpnService::VpnConfiguration* configuration,
    const std::string& service_path,
    const std::string& guid) {
  RegisterConfiguration(configuration, service_path);
  std::move(callback).Run();
}

void VpnService::RegisterConfiguration(
    VpnService::VpnConfiguration* configuration,
    const std::string& service_path) {
  configuration->set_service_path(service_path);
  auto [_, inserted] =
      service_path_to_configuration_map_.emplace(service_path, configuration);
  CHECK(inserted);
  ash::ShillThirdPartyVpnDriverClient::Get()->AddShillThirdPartyVpnObserver(
      configuration->object_path(), configuration);
}

void VpnService::OnCreateConfigurationFailure(
    FailureCallback callback,
    VpnService::VpnConfiguration* configuration,
    const std::string& error_name) {
  DestroyConfigurationInternal(configuration);
  std::move(callback).Run(error_name, /*error_message=*/{});
}

std::string VpnService::GetKeyForTesting(
    const std::string& extension_id,
    const std::string& configuration_name) {
  return GetKey(extension_id, configuration_name);
}

}  // namespace chromeos
