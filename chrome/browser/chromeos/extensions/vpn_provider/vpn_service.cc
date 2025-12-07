// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service.h"

#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/vpn_service_ash.h"
#include "chrome/common/extensions/api/vpn_provider.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_driver_client.h"
#include "content/public/browser/browser_context.h"
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

void RunSuccessCallback(chromeos::VpnService::SuccessCallback success) {
  std::move(success).Run();
}

void RunFailureCallback(chromeos::VpnService::FailureCallback failure,
                        const std::optional<std::string>& error_name,
                        const std::optional<std::string>& error_message) {
  std::move(failure).Run(error_name.value_or(std::string{}),
                         error_message.value_or(std::string{}));
}

using SuccessOrFailureCallback =
    base::OnceCallback<void(crosapi::mojom::VpnErrorResponsePtr)>;

// crosapi::mojom::VpnService expects a single callback, whereas the API is
// designed to pass in two (one for success, one for failure). This function
// glues the two callbacks in one; for the reverse transformation see
// chrome/browser/ash/crosapi/vpn_service_ash.cc
SuccessOrFailureCallback AdaptCallback(
    chromeos::VpnService::SuccessCallback success,
    chromeos::VpnService::FailureCallback failure) {
  return base::BindOnce(
      [](chromeos::VpnService::SuccessCallback success,
         chromeos::VpnService::FailureCallback failure,
         crosapi::mojom::VpnErrorResponsePtr error) {
        if (error) {
          RunFailureCallback(std::move(failure), error->name, error->message);
        } else {
          RunSuccessCallback(std::move(success));
        }
      },
      std::move(success), std::move(failure));
}

bool IsVpnProvider(const extensions::Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kVpnProvider);
}

}  // namespace

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
    : browser_context_(browser_context) {
  auto* registry = extensions::ExtensionRegistry::Get(browser_context);
  extension_registry_observer_.Observe(registry);

  auto* event_router = extensions::EventRouter::Get(browser_context);
  for (const char* event_name : kEventNames) {
    event_router->RegisterObserver(this, event_name);
  }
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

void VpnService::CreateConfiguration(const std::string& extension_id,
                                     const std::string& configuration_name,
                                     SuccessCallback success,
                                     FailureCallback failure) {
  GetVpnServiceForExtension(extension_id)
      ->CreateConfiguration(
          configuration_name,
          AdaptCallback(std::move(success), std::move(failure)));
}

void VpnService::DestroyConfiguration(const std::string& extension_id,
                                      const std::string& configuration_name,
                                      SuccessCallback success,
                                      FailureCallback failure) {
  GetVpnServiceForExtension(extension_id)
      ->DestroyConfiguration(
          configuration_name,
          AdaptCallback(std::move(success), std::move(failure)));
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
          ? base::to_underlying(
                extensions::api::vpn_provider::VpnConnectionState::kConnected)
          : base::to_underlying(
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

}  // namespace chromeos
