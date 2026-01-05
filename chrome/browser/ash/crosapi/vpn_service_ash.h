// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_VPN_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_VPN_SERVICE_ASH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/common/extensions/api/vpn_provider.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_observer.h"
#include "chromeos/ash/components/network/network_configuration_observer.h"
#include "chromeos/crosapi/mojom/vpn_service.mojom.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
class NetworkConfigurationHandler;
}  // namespace ash

namespace chromeos {

class VpnService;

// Fwd for friend declaration in VpnServiceAsh.
class VpnProviderApiTest;

}  // namespace chromeos

namespace crosapi {

namespace api_vpn = extensions::api::vpn_provider;

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
  explicit VpnServiceForExtensionAsh(const std::string& extension_id,
                                     chromeos::VpnService* controller);
  ~VpnServiceForExtensionAsh() override;

  VpnServiceForExtensionAsh(const VpnServiceForExtensionAsh&) = delete;
  VpnServiceForExtensionAsh& operator=(const VpnServiceForExtensionAsh&) =
      delete;

  void BindReceiverAndObserver(
      mojo::PendingReceiver<crosapi::mojom::VpnServiceForExtension> receiver,
      mojo::PendingRemote<crosapi::mojom::EventObserverForExtension> observer);

  // ash::NetworkConfigurationObserver:
  void OnConfigurationRemoved(const std::string& service_path,
                              const std::string& guid) override;

  void DispatchConfigRemovedEvent(const std::string& configuration_name);
  void DispatchOnPacketReceivedEvent(const std::vector<char>& data);

 private:
  friend class VpnConfigurationImpl;
  friend class chromeos::VpnProviderApiTest;
  friend class chromeos::VpnService;
  friend class TestShillControllerAsh;

  const extensions::ExtensionId& extension_id() const { return extension_id_; }

  void DispatchOnPlatformMessageEvent(const std::string& configuration_name,
                                      int32_t platform_message);

  const extensions::ExtensionId extension_id_;
  raw_ptr<chromeos::VpnService> controller_;

  base::ScopedObservation<ash::NetworkConfigurationHandler,
                          ash::NetworkConfigurationObserver>
      network_configuration_observer_{this};

  mojo::ReceiverSet<crosapi::mojom::VpnServiceForExtension> receivers_;
  mojo::RemoteSet<crosapi::mojom::EventObserverForExtension> observers_;

  base::WeakPtrFactory<VpnServiceForExtensionAsh> weak_factory_{this};
};

class VpnServiceAsh : public crosapi::mojom::VpnService {
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

  // Always returns a valid pointer.
  VpnServiceForExtensionAsh* GetVpnServiceForExtension(
      const std::string& extension_id);

  void SetController(chromeos::VpnService* controller) {
    controller_ = controller;
  }

  void Reset();

 private:
  friend class chromeos::VpnProviderApiTest;
  friend class VpnServiceForExtensionAsh;

  // Supports any number of receivers.
  mojo::ReceiverSet<crosapi::mojom::VpnService> receivers_;

  // Maps |extension_id| to a dedicated service for that extension.
  // We do not remove entries from this map due to various ash/lacros corner
  // cases.
  base::flat_map<std::string, std::unique_ptr<VpnServiceForExtensionAsh>>
      extension_id_to_service_;

  raw_ptr<chromeos::VpnService> controller_ = nullptr;
  base::WeakPtrFactory<VpnServiceAsh> weak_factory_{this};
};

class VpnServiceForExtensionAsh::VpnConfiguration
    : public ash::ShillThirdPartyVpnObserver {
 public:
  virtual const std::string& extension_id() const = 0;
  virtual const std::string& configuration_name() const = 0;
  virtual const std::string& key() const = 0;
  virtual const std::string& object_path() const = 0;

  virtual const std::optional<std::string>& service_path() const = 0;
  virtual void set_service_path(std::string) = 0;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_VPN_SERVICE_ASH_H_
