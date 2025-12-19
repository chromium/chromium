// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/vpn_service_ash.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_interface.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/vpn_providers_observer.h"
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

namespace ash {
class NetworkStateHandler;
}  // namespace ash

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class ExtensionRegistry;
}  // namespace extensions

namespace crosapi {
class VpnServiceAsh;
}  // namespace crosapi

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
  void OnConfigRemoved(const std::string& configuration_name) override;
  void OnPlatformMessage(const std::string& configuration_name,
                         int32_t platform_message) override;
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
                   public ash::NetworkStateHandlerObserver,
                   public ash::VpnProvidersObserver::Delegate,
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
  void Shutdown() override;

  // ash::NetworkStateHandlerObserver:
  void NetworkListChanged() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext*,
                              const extensions::Extension*,
                              extensions::UninstallReason) override;
  void OnExtensionUnloaded(content::BrowserContext*,
                           const extensions::Extension*,
                           extensions::UnloadedExtensionReason) override;

  // EventRouter::Observer:
  void OnListenerAdded(const extensions::EventListenerInfo&) override;

  class VpnConfiguration;

 private:
  friend class VpnProviderApiTest;
  friend class VpnServiceForExtension;
  friend class VpnServiceFactory;
  // We are dismantling the crosapi VpnService (crbug.com/365902693).
  friend class crosapi::VpnServiceForExtensionAsh;

  static crosapi::VpnServiceAsh* GetVpnService();

  mojo::Remote<crosapi::mojom::VpnServiceForExtension>&
  GetVpnServiceForExtension(const std::string& extension_id);

  // Looks up the configuration identified by the given name and the extension
  // it belongs to.
  crosapi::VpnServiceForExtensionAsh::VpnConfiguration* LookupConfiguration(
      const std::string& extension_id,
      const std::string& configuration_name);

  // Sends the given event to the given extension.
  void SendToExtension(const std::string& extension_id,
                       std::unique_ptr<extensions::Event> event);

  bool OwnsActiveConfiguration(const std::string& extension_id) const;
  std::optional<std::string> GetActiveConfigurationObjectPath(
      const std::string& extension_id) const;

  void SendOnPlatformMessageToExtension(const std::string& extension_id,
                                        const std::string& configuration_name,
                                        uint32_t platform_message);

  crosapi::VpnServiceForExtensionAsh::VpnConfiguration*
  CreateConfigurationInternal(const std::string& extension_id,
                              const std::string& configuration_name);

  // Callback used to indicate that configuration creation succeeded.
  void OnCreateConfigurationSuccess(
      SuccessCallback callback,
      crosapi::VpnServiceForExtensionAsh::VpnConfiguration* configuration,
      const std::string& service_path,
      const std::string&);

  // Callback used to indicate that configuration creation failed.
  void OnCreateConfigurationFailure(
      FailureCallback callback,
      crosapi::VpnServiceForExtensionAsh::VpnConfiguration* configuration,
      const std::string& error_name);

  // ash::VpnProvidersObserver::Delegate:
  void OnVpnExtensionsChanged(
      base::flat_set<std::string> vpn_extensions) override;

  // Callback for
  // ash::NetworkConfigurationHandler::GetShillProperties(...); parses
  // the |configuration_properties| dictionary and tries to add a new
  // configuration provided that it belongs to some enabled extension.
  void OnGetShillProperties(
      const std::string& service_path,
      std::optional<base::Value::Dict> configuration_properties);

  // Owns all configurations. Key is a hash of |extension_id| and
  // |configuration_name|.
  using StringToOwnedConfigurationMap = std::map<
      std::string,
      std::unique_ptr<crosapi::VpnServiceForExtensionAsh::VpnConfiguration>>;
  StringToOwnedConfigurationMap key_to_configuration_map_;

  raw_ptr<content::BrowserContext> browser_context_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};

  base::flat_map<std::string, std::unique_ptr<VpnServiceForExtension>>
      extension_id_to_service_;

  ash::VpnProvidersObserver vpn_providers_observer_;

  // Ids of enabled vpn extensions.
  base::flat_set<std::string> vpn_extensions_;

  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::WeakPtrFactory<VpnService> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_
