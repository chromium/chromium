// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_interface.h"
#include "chromeos/network/network_configuration_observer.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {

class BrowserContext;
class PepperVpnProviderResourceHostProxy;
class VpnServiceProxy;

}  // namespace content

namespace extensions {

class EventRouter;
class ExtensionRegistry;

}  // namespace extensions

namespace chromeos {

class NetworkConfigurationHandler;
class NetworkProfileHandler;
class NetworkStateHandler;
class ShillThirdPartyVpnDriverClient;

// The class manages the VPN configurations.
class VpnService : public extensions::api::VpnServiceInterface,
                   public NetworkConfigurationObserver,
                   public NetworkStateHandlerObserver,
                   public extensions::ExtensionRegistryObserver {
 public:
  VpnService(content::BrowserContext* browser_context,
             const std::string& userid_hash,
             extensions::ExtensionRegistry* extension_registry,
             extensions::EventRouter* event_router,
             ShillThirdPartyVpnDriverClient* shill_client,
             NetworkConfigurationHandler* network_configuration_handler,
             NetworkProfileHandler* network_profile_handler,
             NetworkStateHandler* network_state_handler);

  VpnService(const VpnService&) = delete;
  VpnService& operator=(const VpnService&) = delete;

  ~VpnService() override;

  void SendPlatformError(const std::string& extension_id,
                         const std::string& configuration_name,
                         const std::string& error_message);

  // NetworkConfigurationObserver:
  void OnConfigurationRemoved(const std::string& service_path,
                              const std::string& guid) override;

  // NetworkStateHandlerObserver:
  void NetworkListChanged() override;

  // ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // extensions::api::VpnServiceThinClientDelegate:
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
  void NotifyConnectionStateChanged(
      const std::string& extension_id,
      extensions::api::vpn_provider::VpnConnectionState,
      SuccessCallback,
      FailureCallback) override;
  std::unique_ptr<content::VpnServiceProxy> GetVpnServiceProxy() override;

  // Verifies if a configuration with name |configuration_name| exists for the
  // extension with id |extension_id|.
  bool VerifyConfigExistsForTesting(const std::string& extension_id,
                                    const std::string& configuration_name);

  // Verifies if the extension has a configuration that is connected.
  bool VerifyConfigIsConnectedForTesting(const std::string& extension_id);

  // Gets the unique key for the configuration |configuration_name| created by
  // the extension with id |extension_id|.
  // This method is made public for testing.
  static std::string GetKey(const std::string& extension_id,
                            const std::string& configuration_name);

  // Returns the single entry of |service_path_to_configuration_map_| for
  // testing (see VpnProviderApiTest);
  const std::string GetSingleServicepathForTesting();

 private:
  class VpnConfiguration;
  class VpnServiceProxyImpl;

  using StringToConfigurationMap = std::map<std::string, VpnConfiguration*>;
  using StringToOwnedConfigurationMap =
      std::map<std::string, std::unique_ptr<VpnConfiguration>>;

  // Callback used to indicate that configuration was successfully created.
  void OnCreateConfigurationSuccess(SuccessCallback callback,
                                    VpnConfiguration* configuration,
                                    const std::string& service_path,
                                    const std::string& guid);

  // Callback used to indicate that configuration creation failed.
  void OnCreateConfigurationFailure(FailureCallback callback,
                                    VpnConfiguration* configuration,
                                    const std::string& error_name);

  // Callback used to indicate that removing a configuration succeeded.
  void OnRemoveConfigurationSuccess(SuccessCallback callback);

  // Callback used to indicate that removing a configuration failed.
  void OnRemoveConfigurationFailure(FailureCallback callback,
                                    const std::string& error_name);

  void OnGetShillProperties(const std::string& service_path,
                            absl::optional<base::Value> dictionary);

  // Creates and adds the configuration to the internal store.
  VpnConfiguration* CreateConfigurationInternal(
      const std::string& extension_id,
      const std::string& configuration_name,
      const std::string& key);

  // Removes configuration from the internal store and destroys it.
  void DestroyConfigurationInternal(VpnConfiguration* configuration);

  // Verifies if |active_configuration_| exists and if the extension with id
  // |extension_id| is authorized to access it.
  bool DoesActiveConfigurationExistAndIsAccessAuthorized(
      const std::string& extension_id);

  // Send an event with name |event_name| and arguments |event_args| to the
  // extension with id |extension_id|.
  void SendSignalToExtension(const std::string& extension_id,
                             extensions::events::HistogramValue histogram_value,
                             const std::string& event_name,
                             std::vector<base::Value> event_args);

  // Destroy configurations belonging to the extension.
  void DestroyConfigurationsForExtension(
      const extensions::Extension* extension);

  // Set the active configuration.
  void SetActiveConfiguration(VpnConfiguration* configuration);

  void Bind(const std::string& extension_id,
            const std::string& configuration_id,
            const std::string& configuration_name,
            SuccessCallback success,
            FailureCallback failure,
            std::unique_ptr<content::PepperVpnProviderResourceHostProxy>
                pepper_vpn_provider_proxy);

  content::BrowserContext* browser_context_;
  std::string userid_hash_;

  extensions::ExtensionRegistry* extension_registry_;
  extensions::EventRouter* event_router_;
  ShillThirdPartyVpnDriverClient* shill_client_;
  NetworkConfigurationHandler* network_configuration_handler_;
  NetworkProfileHandler* network_profile_handler_;
  NetworkStateHandler* network_state_handler_;

  VpnConfiguration* active_configuration_;

  StringToOwnedConfigurationMap key_to_configuration_map_;

  // Service path does not own the VpnConfigurations.
  StringToConfigurationMap service_path_to_configuration_map_;

  base::WeakPtrFactory<VpnService> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_
