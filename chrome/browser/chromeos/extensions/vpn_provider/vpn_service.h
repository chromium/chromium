// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_interface.h"
#include "chromeos/crosapi/mojom/vpn_service.mojom.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class BrowserContext;
class PepperVpnProviderResourceHostProxy;

}  // namespace content

namespace extensions {

class ExtensionRegistry;

}  // namespace extensions

namespace chromeos {

class VpnServiceForExtension
    : public crosapi::mojom::EventObserverForExtension {
 public:
  VpnServiceForExtension(const std::string& extension_id,
                         content::BrowserContext*);
  ~VpnServiceForExtension() override;

  VpnServiceForExtension(const VpnServiceForExtension&) = delete;
  VpnServiceForExtension& operator=(const VpnServiceForExtension&) = delete;

  // crosapi::mojom::EventObserverForExtension:
  void OnAddDialog() override;
  void OnConfigureDialog(const std::string& configuration_name) override;
  void OnConfigRemoved(const std::string& configuration_name) override;
  void OnPlatformMessage(const std::string& configuration_name,
                         int32_t platform_message,
                         const std::optional<std::string>& error) override;
  void OnPacketReceived(const std::vector<uint8_t>& data) override;

  mojo::Remote<crosapi::mojom::VpnServiceForExtension>& Proxy() {
    return vpn_service_;
  }

 private:
  void DispatchEvent(std::unique_ptr<extensions::Event>) const;

  const extensions::ExtensionId extension_id_;
  raw_ptr<content::BrowserContext> browser_context_;

  mojo::Remote<crosapi::mojom::VpnServiceForExtension> vpn_service_;
  mojo::Receiver<crosapi::mojom::EventObserverForExtension> receiver_{this};
};

// The class manages the VPN configurations.
class VpnService : public extensions::api::VpnServiceInterface,
                   public extensions::ExtensionRegistryObserver,
                   public extensions::EventRouter::Observer {
 public:
  explicit VpnService(content::BrowserContext*);
  ~VpnService() override;

  VpnService(const VpnService&) = delete;
  VpnService& operator=(const VpnService&) = delete;

  // extensions::api::VpnServiceInterface:
  void SendShowAddDialogToExtension(const std::string& extension_id) override;
  void SendShowConfigureDialogToExtension(
      const std::string& extension_id,
      const std::string& configuration_name) override;
  void CreateConfiguration(const std::string& extension_id,
                           const std::string& configuration_name,
                           SuccessCallback,
                           FailureCallback) override;
  void DestroyConfiguration(const std::string& extension_id,
                            const std::string& configuration_id,
                            SuccessCallback,
                            FailureCallback) override;
  void SetParameters(const std::string& extension_id,
                     base::Value::Dict parameters,
                     SuccessCallback,
                     FailureCallback) override;
  void SendPacket(const std::string& extension_id,
                  const std::vector<char>& data,
                  SuccessCallback,
                  FailureCallback) override;
  void NotifyConnectionStateChanged(const std::string& extension_id,
                                    bool connection_success,
                                    SuccessCallback,
                                    FailureCallback) override;
  std::unique_ptr<content::VpnServiceProxy> GetVpnServiceProxy() override;
  void Shutdown() override;

  // ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext*,
                              const extensions::Extension*,
                              extensions::UninstallReason) override;
  void OnExtensionUnloaded(content::BrowserContext*,
                           const extensions::Extension*,
                           extensions::UnloadedExtensionReason) override;

  // EventRouter::Observer:
  void OnListenerAdded(const extensions::EventListenerInfo&) override;

 private:
  class VpnServiceProxyImpl;
  class PepperVpnProxyAdapter;
  friend class VpnServiceForExtension;
  friend class VpnServiceFactory;

  static crosapi::mojom::VpnService* GetVpnService();

  mojo::Remote<crosapi::mojom::VpnServiceForExtension>&
  GetVpnServiceForExtension(const std::string& extension_id);

  // Binds |pepper_vpn_provider_proxy| to the active configuration if it's owned
  // by extension with id |extension_id|. On success all packets will be routed
  // through Pepper API.
  void BindPepperVpnProxy(
      const std::string& extension_id,
      const std::string& configuration_name,
      SuccessCallback,
      FailureCallback,
      std::unique_ptr<content::PepperVpnProviderResourceHostProxy>);

  void OnBindPepperVpnProxy(
      SuccessCallback,
      FailureCallback,
      std::unique_ptr<PepperVpnProxyAdapter>,
      mojo::PendingReceiver<crosapi::mojom::PepperVpnProxyObserver>,
      crosapi::mojom::VpnErrorResponsePtr);

  raw_ptr<content::BrowserContext> browser_context_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};

  base::flat_map<std::string, std::unique_ptr<VpnServiceForExtension>>
      extension_id_to_service_;

  base::WeakPtrFactory<VpnService> weak_factory_{this};
};

// Listens to incoming events and forwards them to the underlying
// PepperVpnProviderResourseHostProxy.
// * ::OnUnbind() -> pepper->SendOnUnbind()
// * ::OnPacketReceived(...) -> pepper->SendOnPacketReceived(...)
class VpnService::PepperVpnProxyAdapter
    : public crosapi::mojom::PepperVpnProxyObserver {
 public:
  explicit PepperVpnProxyAdapter(
      std::unique_ptr<content::PepperVpnProviderResourceHostProxy>);
  ~PepperVpnProxyAdapter() override;

  // crosapi::mojom::PepperVpnProxyObserver:
  void OnUnbind() override;
  void OnPacketReceived(const std::vector<uint8_t>& data) override;

 private:
  std::unique_ptr<content::PepperVpnProviderResourceHostProxy>
      pepper_vpn_proxy_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_
