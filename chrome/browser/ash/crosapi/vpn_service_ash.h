// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_VPN_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_VPN_SERVICE_ASH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/common/extensions/api/vpn_provider.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_observer.h"
#include "chromeos/ash/components/network/network_configuration_observer.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/crosapi/mojom/vpn_service.mojom.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
class NetworkConfigurationHandler;
class NetworkStateHandler;
}  // namespace ash

namespace base {
class Value;
}  // namespace base

namespace chromeos {

// Fwd for friend declaration in VpnServiceAsh.
class VpnProviderApiTestAsh;

}  // namespace chromeos

namespace crosapi {

namespace api_vpn = extensions::api::vpn_provider;

// Listens to |OnVpnProvidersChanged| event and informs the delegate of the
// current set of vpn extension.
class VpnProvidersObserver
    : public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnVpnExtensionsChanged(
        base::flat_set<std::string> vpn_extensions) = 0;
  };

  explicit VpnProvidersObserver(Delegate*);
  ~VpnProvidersObserver() override;

  // ash::network_config::CrosNetworkConfigObserver:
  void OnVpnProvidersChanged() override;

 private:
  // Callback for CrosNetworkConfig::GetVpnProviders().
  // Extracts vpn extension ids and calls Delegate::OnVpnExtensionsChanged().
  void OnGetVpnProviders(
      std::vector<chromeos::network_config::mojom::VpnProviderPtr>
          vpn_providers);

  raw_ptr<Delegate> delegate_ = nullptr;

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_{this};

  base::WeakPtrFactory<VpnProvidersObserver> weak_factory_{this};
};

class VpnServiceAsh;

// This class manages configurations for a particular extension.
class VpnServiceForExtensionAsh : public crosapi::mojom::VpnServiceForExtension,
                                  public ash::NetworkConfigurationObserver {
 public:
  // Callback definitions.
  using SuccessCallback = base::OnceClosure;
  using FailureCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;

  // Class forward declarations.
  class VpnConfiguration;

 public:
  explicit VpnServiceForExtensionAsh(const std::string& extension_id);
  ~VpnServiceForExtensionAsh() override;

  VpnServiceForExtensionAsh(const VpnServiceForExtensionAsh&) = delete;
  VpnServiceForExtensionAsh& operator=(const VpnServiceForExtensionAsh&) =
      delete;

  void BindReceiverAndObserver(
      mojo::PendingReceiver<crosapi::mojom::VpnServiceForExtension> receiver,
      mojo::PendingRemote<crosapi::mojom::EventObserverForExtension> observer);

  // crosapi::mojom::VpnServiceForExtension:
  void CreateConfiguration(const std::string& configuration_name,
                           CreateConfigurationCallback) override;
  void DestroyConfiguration(const std::string& configuration_name,
                            DestroyConfigurationCallback) override;
  void SetParameters(base::Value::Dict parameters,
                     SetParametersCallback) override;
  void SendPacket(const std::vector<uint8_t>& data,
                  SendPacketCallback) override;
  void NotifyConnectionStateChanged(
      bool connection_success,
      NotifyConnectionStateChangedCallback) override;
  void BindPepperVpnProxyObserver(
      const std::string& configuration_name,
      mojo::PendingRemote<crosapi::mojom::PepperVpnProxyObserver>
          pepper_vpn_proxy_observer,
      BindPepperVpnProxyObserverCallback) override;
  void DispatchAddDialogEvent() override;
  void DispatchConfigureDialogEvent(
      const std::string& configuration_name) override;

  // ash::NetworkConfigurationObserver:
  void OnConfigurationRemoved(const std::string& service_path,
                              const std::string& guid) override;

  bool OwnsActiveConfiguration() const;
  bool HasConfigurationForServicePath(const std::string& service_path) const;

  void DestroyAllConfigurations();

  void CreateConfigurationWithServicePath(const std::string& configuration_name,
                                          const std::string& service_path);

  void DispatchConfigRemovedEvent(const std::string& configuration_name);
  void DispatchOnPacketReceivedEvent(const std::vector<char>& data);
  void DispatchOnPlatformMessageEvent(
      const std::string& configuration_name,
      int32_t platform_message,
      const std::optional<std::string>& error = {});

 private:
  friend class VpnConfigurationImpl;
  friend class chromeos::VpnProviderApiTestAsh;
  friend class TestShillControllerAsh;

  using StringToOwnedConfigurationMap =
      std::map<std::string, std::unique_ptr<VpnConfiguration>>;
  using StringToConfigurationMap =
      std::map<std::string, raw_ptr<VpnConfiguration, CtnExperimental>>;

  const extensions::ExtensionId& extension_id() const { return extension_id_; }

  // Creates a key for |key_to_configuration_map_| as a hash of |extension_id|
  // and |configuration_name|.
  static std::string GetKey(const std::string& extension_id,
                            const std::string& configuration_name);

  // Creates and adds the configuration to the internal store.
  VpnConfiguration* CreateConfigurationInternal(
      const std::string& configuration_name);

  // Removes configuration from the internal store and destroys it.
  void DestroyConfigurationInternal(VpnConfiguration*);

  // Callback used to indicate that configuration was successfully created.
  void OnCreateConfigurationSuccess(SuccessCallback,
                                    VpnConfiguration*,
                                    const std::string& service_path,
                                    const std::string& guid);

  // Callback used to indicate that configuration creation failed.
  void OnCreateConfigurationFailure(FailureCallback,
                                    VpnConfiguration*,
                                    const std::string& error_name);

  // Callback used to indicate that removing a configuration succeeded.
  void OnRemoveConfigurationSuccess(SuccessCallback);

  // Callback used to indicate that removing a configuration failed.
  void OnRemoveConfigurationFailure(FailureCallback,
                                    const std::string& error_name);

  void SetActiveConfiguration(VpnConfiguration*);

  const extensions::ExtensionId extension_id_;

  // Owns all configurations. Key is a hash of |extension_id| and
  // |configuration_name|.
  StringToOwnedConfigurationMap key_to_configuration_map_;
  // Maps shill service path to unowned configuration.
  StringToConfigurationMap service_path_to_configuration_map_;

  // Configuration that is currently in use.
  raw_ptr<VpnConfiguration> active_configuration_ = nullptr;

  base::ScopedObservation<ash::NetworkConfigurationHandler,
                          ash::NetworkConfigurationObserver>
      network_configuration_observer_{this};

  mojo::ReceiverSet<crosapi::mojom::VpnServiceForExtension> receivers_;
  mojo::RemoteSet<crosapi::mojom::EventObserverForExtension> observers_;

  base::WeakPtrFactory<VpnServiceForExtensionAsh> weak_factory_{this};
};

class VpnServiceAsh : public crosapi::mojom::VpnService,
                      public ash::NetworkStateHandlerObserver,
                      public VpnProvidersObserver::Delegate {
 public:
  VpnServiceAsh();
  ~VpnServiceAsh() override;

  VpnServiceAsh(const VpnServiceAsh&) = delete;
  VpnServiceAsh& operator=(const VpnServiceAsh&) = delete;

  // Binds |receiver| to this instance of VpnServiceAsh.
  void BindReceiver(mojo::PendingReceiver<crosapi::mojom::VpnService> receiver);

  // crosapi::mojom::VpnService:
  void RegisterVpnServiceForExtension(
      const std::string& extension_id,
      mojo::PendingReceiver<crosapi::mojom::VpnServiceForExtension> receiver,
      mojo::PendingRemote<crosapi::mojom::EventObserverForExtension> observer)
      override;
  void MaybeFailActiveConnectionAndDestroyConfigurations(
      const std::string& extension_id,
      bool destroy_configurations) override;

  // ash::NetworkStateHandlerObserver:
  void NetworkListChanged() override;

  // VpnProvidersObserver::Delegate:
  void OnVpnExtensionsChanged(
      base::flat_set<std::string> vpn_extensions) override;

 private:
  friend class chromeos::VpnProviderApiTestAsh;
  friend class VpnServiceForExtensionAsh;

  // Callback for
  // ash::NetworkConfigurationHandler::GetShillProperties(...); parses
  // the |configuration_properties| dictionary and tries to add a new
  // configuration provided that it belongs to some enabled extension.
  void OnGetShillProperties(
      const std::string& service_path,
      std::optional<base::Value::Dict> configuration_properties);

  // Always returns a valid pointer.
  VpnServiceForExtensionAsh* GetVpnServiceForExtension(
      const std::string& extension_id);

  // Ids of enabled vpn extensions.
  base::flat_set<std::string> vpn_extensions_;

  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  // Supports any number of receivers.
  mojo::ReceiverSet<crosapi::mojom::VpnService> receivers_;

  // Maps |extension_id| to a dedicated service for that extension.
  // We do not remove entries from this map due to various ash/lacros corner
  // cases.
  base::flat_map<std::string, std::unique_ptr<VpnServiceForExtensionAsh>>
      extension_id_to_service_;

  std::unique_ptr<VpnProvidersObserver> vpn_providers_observer_;

  base::WeakPtrFactory<VpnServiceAsh> weak_factory_{this};
};

class VpnServiceForExtensionAsh::VpnConfiguration
    : public ash::ShillThirdPartyVpnObserver {
 public:
  virtual const std::string& configuration_name() const = 0;
  virtual const std::string& key() const = 0;
  virtual const std::string& object_path() const = 0;

  virtual const std::optional<std::string>& service_path() const = 0;
  virtual void set_service_path(std::string) = 0;

  virtual void BindPepperVpnProxyObserver(
      mojo::PendingRemote<crosapi::mojom::PepperVpnProxyObserver>) = 0;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_VPN_SERVICE_ASH_H_
