// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_configuration_observer.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/unloaded_extension_reason.h"

namespace ash {
class NetworkConfigurationHandler;
class NetworkStateHandler;
}  // namespace ash

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
struct Event;
class ExtensionRegistry;
}  // namespace extensions

namespace chromeos {

// The class manages the VPN configurations.
class VpnService : public KeyedService,
                   public ash::NetworkConfigurationObserver,
                   public ash::NetworkStateHandlerObserver,
                   public extensions::ExtensionRegistryObserver {
 public:
  using SuccessCallback = base::OnceClosure;
  using FailureCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;

  explicit VpnService(content::BrowserContext*);
  ~VpnService() override;

  VpnService(const VpnService&) = delete;
  VpnService& operator=(const VpnService&) = delete;

  // Sends UIEvent.showAddDialog to the extension with `extension_id`.
  void SendShowAddDialogToExtension(const std::string& extension_id);

  // Sends UIEvent.showConfigureDialog for `configuration_name` to
  // the extension with `extension_id`.
  void SendShowConfigureDialogToExtension(
      const std::string& extension_id,
      const std::string& configuration_name);

  // Creates a new VPN configuration with `configuration_name` as the name and
  // attaches it to the extension with id `extension_id`.
  // Calls `success` or `failure` based on the outcome.
  void CreateConfiguration(const std::string& extension_id,
                           const std::string& configuration_name,
                           SuccessCallback success,
                           FailureCallback failure);

  // Destroys the VPN configuration with `configuration_id` after verifying
  // that it belongs to the extension with id `extension_id`. Calls `success` or
  // `failure` based on the outcome.
  void DestroyConfiguration(const std::string& extension_id,
                            const std::string& configuration_id,
                            SuccessCallback success,
                            FailureCallback failure);

  // Set `parameters` for the active VPN configuration after verifying that it
  // belongs to the extension with id `extension_id`.
  // Calls `success` or `failure` based on the outcome.
  void SetParameters(const std::string& extension_id,
                     base::DictValue parameters,
                     SuccessCallback success,
                     FailureCallback failure);

  // Sends an IP packet contained in `data` to the active VPN configuration
  // after verifying that it belongs to the extension with id `extension_id`.
  // Calls `success` or `failure` based on the outcome.
  void SendPacket(const std::string& extension_id,
                  const std::vector<char>& data,
                  SuccessCallback success,
                  FailureCallback failure);

  // Notifies new connection state to the active VPN configuration after
  // verifying that it belongs to the extension with id `extension_id`.
  // Calls `success` or `failure` based on the outcome.
  void NotifyConnectionStateChanged(const std::string& extension_id,
                                    bool connection_success,
                                    SuccessCallback success,
                                    FailureCallback failure);
  // ash::NetworkConfigurationObserver:
  void OnConfigurationRemoved(const std::string& service_path,
                              const std::string& guid) override;

  // ash::NetworkStateHandlerObserver:
  void NetworkListChanged() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext*,
                              const extensions::Extension*,
                              extensions::UninstallReason) override;
  void OnExtensionUnloaded(content::BrowserContext*,
                           const extensions::Extension*,
                           extensions::UnloadedExtensionReason) override;

  class VpnConfiguration;

 private:
  friend class VpnProviderApiTest;
  friend class VpnServiceFactory;

  // Looks up the configuration identified by the given service path.
  VpnConfiguration* LookupConfiguration(const std::string& service_path);

  // Looks up the configuration identified by the given name and the extension
  // it belongs to.
  VpnConfiguration* LookupConfiguration(const std::string& extension_id,
                                        const std::string& configuration_name);

  // Sets the active configuration.
  void SetActiveConfiguration(VpnConfiguration* configuration);

  VpnConfiguration* GetActiveConfigurationForExtension(
      const std::string& extension_id) const;

  // Sends the given event to the given extension.
  void SendToExtension(const std::string& extension_id,
                       std::unique_ptr<extensions::Event> event);

  void SendOnPacketReceivedToExtension(const std::string& extension_id,
                                       const std::vector<char>& data);
  void SendOnPlatformMessageToExtension(const std::string& extension_id,
                                        const std::string& configuration_name,
                                        uint32_t platform_message);
  void SendOnConfigRemovedToExtension(const std::string& extension_id,
                                      const std::string& configuration_name);

  VpnConfiguration* GetOrCreateConfigurationInternal(
      const std::string& extension_id,
      const std::string& configuration_name);

  void DestroyConfigurationsForExtension(const std::string& extension_id);

  // Callback used to indicate that configuration creation succeeded.
  void OnCreateConfigurationSuccess(SuccessCallback callback,
                                    VpnConfiguration* configuration,
                                    const std::string& service_path,
                                    const std::string&);

  // Callback used to indicate that configuration creation failed.
  void OnCreateConfigurationFailure(FailureCallback callback,
                                    VpnConfiguration* configuration,
                                    const std::string& error_name);

  // Callback for ash::NetworkConfigurationHandler::GetShillProperties that
  // parses the |configuration_properties| dictionary and tries to add a new
  // configuration provided that it belongs to some enabled extension.
  void OnGetShillProperties(
      const std::string& service_path,
      std::optional<base::DictValue> configuration_properties);

  // Sets `configuration`s service path as given and enters it into
  // `service_path_to_configuration_map_`.
  void RegisterConfiguration(VpnConfiguration* configuration,
                             const std::string& service_path);

  // Removes configuration from the internal store and destroys it.
  void DestroyConfigurationInternal(VpnConfiguration* configuration);

  // Callback used to indicate that removing a configuration succeeded.
  void OnRemoveConfigurationSuccess(SuccessCallback);

  // Callback used to indicate that removing a configuration failed.
  void OnRemoveConfigurationFailure(FailureCallback,
                                    const std::string& error_name);

  // Gets the unique key for the configuration |configuration_name| created by
  // the extension with id |extension_id|.
  static std::string GetKeyForTesting(const std::string& extension_id,
                                      const std::string& configuration_name);

  const raw_ref<content::BrowserContext> browser_context_;

  // Owns all configurations. Key is a hash of |extension_id| and
  // |configuration_name|.
  using StringToOwnedConfigurationMap =
      base::flat_map<std::string, std::unique_ptr<VpnConfiguration>>;
  StringToOwnedConfigurationMap key_to_configuration_map_;

  // Maps shill service path to (unowned) configuration.
  using StringToConfigurationMap =
      base::flat_map<std::string, raw_ptr<VpnConfiguration, CtnExperimental>>;
  StringToConfigurationMap service_path_to_configuration_map_;

  // Configuration that is currently in use.
  raw_ptr<VpnConfiguration> active_configuration_ = nullptr;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};

  base::ScopedObservation<ash::NetworkConfigurationHandler,
                          ash::NetworkConfigurationObserver>
      network_configuration_observer_{this};

  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::WeakPtrFactory<VpnService> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_VPN_PROVIDER_VPN_SERVICE_H_
