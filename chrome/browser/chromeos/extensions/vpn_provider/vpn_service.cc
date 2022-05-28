// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/pepper_vpn_provider_resource_host_proxy.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/vpn_service_ash.h"

namespace chromeos {

namespace {

namespace api_vpn = extensions::api::vpn_provider;

void RunSuccessCallback(chromeos::VpnService::SuccessCallback success) {
  std::move(success).Run();
}

void RunFailureCallback(chromeos::VpnService::FailureCallback failure,
                        const absl::optional<std::string>& error_name,
                        const absl::optional<std::string>& error_message) {
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

}  // namespace

class VpnService::VpnServiceProxyImpl : public content::VpnServiceProxy {
 public:
  explicit VpnServiceProxyImpl(base::WeakPtr<VpnService> vpn_service);

  VpnServiceProxyImpl(const VpnServiceProxyImpl&) = delete;
  VpnServiceProxyImpl& operator=(const VpnServiceProxyImpl&) = delete;

  void Bind(const std::string& extension_id,
            const std::string& configuration_id,
            const std::string& configuration_name,
            SuccessCallback success,
            FailureCallback failure,
            std::unique_ptr<content::PepperVpnProviderResourceHostProxy>
                pepper_vpn_provider_proxy) override;

  void SendPacket(const std::string& extension_id,
                  const std::vector<char>& data,
                  SuccessCallback success,
                  FailureCallback failure) override;

 private:
  base::WeakPtr<VpnService> vpn_service_;
};

VpnService::VpnServiceProxyImpl::VpnServiceProxyImpl(
    base::WeakPtr<VpnService> vpn_service)
    : vpn_service_(vpn_service) {}

void VpnService::VpnServiceProxyImpl::Bind(
    const std::string& extension_id,
    const std::string& /*configuration_id*/,
    const std::string& configuration_name,
    SuccessCallback success,
    FailureCallback failure,
    std::unique_ptr<content::PepperVpnProviderResourceHostProxy>
        pepper_vpn_provider_proxy) {
  if (!vpn_service_) {
    return;
  }

  vpn_service_->BindPepperVpnProxy(extension_id, configuration_name,
                                   std::move(success), std::move(failure),
                                   std::move(pepper_vpn_provider_proxy));
}

void VpnService::VpnServiceProxyImpl::SendPacket(
    const std::string& extension_id,
    const std::vector<char>& data,
    SuccessCallback success,
    FailureCallback failure) {
  if (!vpn_service_) {
    return;
  }

  vpn_service_->SendPacket(extension_id, data, std::move(success),
                           std::move(failure));
}

VpnServiceForExtension::VpnServiceForExtension(
    const std::string& extension_id,
    content::BrowserContext* browser_context)
    : extension_id_(extension_id), browser_context_(browser_context) {
  VpnService::GetVpnService()->RegisterVpnServiceForExtension(
      extension_id, vpn_service_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote());
}

VpnServiceForExtension::~VpnServiceForExtension() = default;

void VpnServiceForExtension::OnAddDialog() {
  DispatchEvent(std::make_unique<extensions::Event>(
      extensions::events::HistogramValue::VPN_PROVIDER_ON_UI_EVENT,
      api_vpn::OnUIEvent::kEventName,
      api_vpn::OnUIEvent::Create(api_vpn::UI_EVENT_SHOWADDDIALOG,
                                 std::string()),
      browser_context_));
}

void VpnServiceForExtension::OnConfigureDialog(
    const std::string& configuration_name) {
  DispatchEvent(std::make_unique<extensions::Event>(
      extensions::events::HistogramValue::VPN_PROVIDER_ON_UI_EVENT,
      api_vpn::OnUIEvent::kEventName,
      api_vpn::OnUIEvent::Create(api_vpn::UI_EVENT_SHOWCONFIGUREDIALOG,
                                 configuration_name),
      browser_context_));
}

void VpnServiceForExtension::OnConfigRemoved(
    const std::string& configuration_name) {
  DispatchEvent(std::make_unique<extensions::Event>(
      extensions::events::HistogramValue::VPN_PROVIDER_ON_CONFIG_REMOVED,
      api_vpn::OnConfigRemoved::kEventName,
      api_vpn::OnConfigRemoved::Create(configuration_name), browser_context_));
}

void VpnServiceForExtension::OnPlatformMessage(
    const std::string& configuration_name,
    int32_t platform_message,
    const absl::optional<std::string>& error) {
  DispatchEvent(std::make_unique<extensions::Event>(
      extensions::events::VPN_PROVIDER_ON_PLATFORM_MESSAGE,
      api_vpn::OnPlatformMessage::kEventName,
      api_vpn::OnPlatformMessage::Create(
          configuration_name,
          static_cast<api_vpn::PlatformMessage>(platform_message),
          error.value_or(std::string{})),
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

void VpnServiceForExtension::DispatchEvent(
    std::unique_ptr<extensions::Event> event) const {
  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id_, std::move(event));
}

VpnService::VpnService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  extension_registry_observer_.Observe(
      extensions::ExtensionRegistry::Get(browser_context));
}

VpnService::~VpnService() = default;

void VpnService::SendShowAddDialogToExtension(const std::string& extension_id) {
  GetVpnServiceForExtension(extension_id)->DispatchAddDialogEvent();
}

void VpnService::SendShowConfigureDialogToExtension(
    const std::string& extension_id,
    const std::string& configuration_name) {
  GetVpnServiceForExtension(extension_id)
      ->DispatchConfigureDialogEvent(configuration_name);
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
  GetVpnServiceForExtension(extension_id)
      ->SetParameters(std::move(parameters),
                      AdaptCallback(std::move(success), std::move(failure)));
}

void VpnService::SendPacket(const std::string& extension_id,
                            const std::vector<char>& data,
                            SuccessCallback success,
                            FailureCallback failure) {
  GetVpnServiceForExtension(extension_id)
      ->SendPacket(std::vector<uint8_t>(data.begin(), data.end()),
                   AdaptCallback(std::move(success), std::move(failure)));
}

void VpnService::NotifyConnectionStateChanged(const std::string& extension_id,
                                              bool connection_success,
                                              SuccessCallback success,
                                              FailureCallback failure) {
  GetVpnServiceForExtension(extension_id)
      ->NotifyConnectionStateChanged(
          connection_success,
          AdaptCallback(std::move(success), std::move(failure)));
}

std::unique_ptr<content::VpnServiceProxy> VpnService::GetVpnServiceProxy() {
  return std::make_unique<VpnServiceProxyImpl>(weak_factory_.GetWeakPtr());
}

void VpnService::OnExtensionUninstalled(content::BrowserContext*,
                                        const extensions::Extension* extension,
                                        extensions::UninstallReason) {
  GetVpnService()->MaybeFailActiveConnectionAndDestroyConfigurations(
      extension->id(), /*destroy_configurations=*/true);
  extension_id_to_service_.erase(extension->id());
}

void VpnService::OnExtensionUnloaded(
    content::BrowserContext*,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  bool destroy_configurations =
      reason == extensions::UnloadedExtensionReason::DISABLE ||
      reason == extensions::UnloadedExtensionReason::BLOCKLIST;
  GetVpnService()->MaybeFailActiveConnectionAndDestroyConfigurations(
      extension->id(), destroy_configurations);
  if (destroy_configurations) {
    extension_id_to_service_.erase(extension->id());
  }
}

// static
crosapi::mojom::VpnService* VpnService::GetVpnService() {
  DCHECK(crosapi::CrosapiManager::IsInitialized());
  return crosapi::CrosapiManager::Get()->crosapi_ash()->vpn_service_ash();
}

mojo::Remote<crosapi::mojom::VpnServiceForExtension>&
VpnService::GetVpnServiceForExtension(const std::string& extension_id) {
  auto it = extension_id_to_service_.find(extension_id);
  if (it == extension_id_to_service_.end()) {
    it = extension_id_to_service_.insert(
        it, {extension_id, std::make_unique<VpnServiceForExtension>(
                               extension_id, browser_context_)});
  }
  const auto& service = it->second;
  return service->Proxy();
}

void VpnService::BindPepperVpnProxy(
    const std::string& extension_id,
    const std::string& configuration_name,
    SuccessCallback success,
    FailureCallback failure,
    std::unique_ptr<content::PepperVpnProviderResourceHostProxy>
        pepper_vpn_provider_proxy) {
  // Here we create a PepperVpnProxyAdapter that will forward everything to the
  // underlying PepperVpnProviderResourceHostProxy and bind it via crosapi. Note
  // that the crosapi call might be unsuccessful if the active vpn configuration
  // is not owned by the given extension; therefore we don't create the
  // SelfOwnedReceiver right away, but instead do it in the callback on success
  // or reset the entangled pipe on failure.
  auto pepper_adapter = std::make_unique<PepperVpnProxyAdapter>(
      std::move(pepper_vpn_provider_proxy));
  mojo::PendingRemote<crosapi::mojom::PepperVpnProxyObserver> pepper_client;

  auto callback = base::BindOnce(
      &VpnService::OnBindPepperVpnProxy, weak_factory_.GetWeakPtr(),
      std::move(success), std::move(failure), std::move(pepper_adapter),
      pepper_client.InitWithNewPipeAndPassReceiver());

  GetVpnServiceForExtension(extension_id)
      ->BindPepperVpnProxyObserver(configuration_name, std::move(pepper_client),
                                   std::move(callback));
}

void VpnService::OnBindPepperVpnProxy(
    SuccessCallback success,
    FailureCallback failure,
    std::unique_ptr<PepperVpnProxyAdapter> pepper_adapter,
    mojo::PendingReceiver<crosapi::mojom::PepperVpnProxyObserver>
        pepper_receiver,
    crosapi::mojom::VpnErrorResponsePtr error) {
  if (error) {
    pepper_receiver.reset();
    RunFailureCallback(std::move(failure), error->name, error->message);
  } else {
    // Gets reset when the active configuration in ash gets destroyed.
    mojo::MakeSelfOwnedReceiver(std::move(pepper_adapter),
                                std::move(pepper_receiver));
    RunSuccessCallback(std::move(success));
  }
}

VpnService::PepperVpnProxyAdapter::PepperVpnProxyAdapter(
    std::unique_ptr<content::PepperVpnProviderResourceHostProxy>
        pepper_vpn_proxy)
    : pepper_vpn_proxy_(std::move(pepper_vpn_proxy)) {}

VpnService::PepperVpnProxyAdapter::~PepperVpnProxyAdapter() = default;

void VpnService::PepperVpnProxyAdapter::OnUnbind() {
  pepper_vpn_proxy_->SendOnUnbind();
  pepper_vpn_proxy_.reset();
}

void VpnService::PepperVpnProxyAdapter::OnPacketReceived(
    const std::vector<uint8_t>& data) {
  DCHECK(pepper_vpn_proxy_);
  pepper_vpn_proxy_->SendOnPacketReceived(
      std::vector<char>(data.begin(), data.end()));
}

}  // namespace chromeos
