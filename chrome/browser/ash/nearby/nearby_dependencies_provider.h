// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_NEARBY_DEPENDENCIES_PROVIDER_H_
#define CHROME_BROWSER_ASH_NEARBY_NEARBY_DEPENDENCIES_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "chromeos/ash/services/wifi_direct/wifi_direct_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"

class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace bluetooth::mojom {
class Adapter;
}  // namespace bluetooth::mojom

namespace ash::nearby {

class BluetoothAdapterManager;

namespace presence {
class CredentialStorageInitializer;
}  // namespace presence

// Provides dependencies required to initialize NearbyPresence and
// NearbyConnections. Implemented as a KeyedService because WebRTC
// dependencies are linked to the user's identity.
class NearbyDependenciesProvider : public KeyedService {
 public:
  NearbyDependenciesProvider(Profile* profile,
                             signin::IdentityManager* identity_manager);
  ~NearbyDependenciesProvider() override;

  // Note: Returns null during session shutdown.
  virtual ::sharing::mojom::NearbyDependenciesPtr GetDependencies();

  virtual void PrepareForShutdown();

  static void EnsureFactoryBuilt();

 private:
  friend class NearbyProcessManagerImplTest;

  // KeyedService:
  void Shutdown() override;

  // Test-only constructor.
  NearbyDependenciesProvider();

  mojo::PendingRemote<::bluetooth::mojom::Adapter>
  GetBluetoothAdapterPendingRemote();

  mojo::PendingRemote<presence::mojom::NearbyPresenceCredentialStorage>
  GetNearbyPresenceCredentialStoragePendingRemote();

  ::sharing::mojom::WebRtcDependenciesPtr GetWebRtcDependencies();

  ::sharing::mojom::WifiLanDependenciesPtr GetWifiLanDependencies();

  sharing::mojom::WifiDirectDependenciesPtr GetWifiDirectDependencies();

  network::mojom::NetworkContext* GetNetworkContext();

  std::unique_ptr<wifi_direct::WifiDirectManager> wifi_direct_manager_;
  std::unique_ptr<BluetoothAdapterManager> bluetooth_manager_;

  std::unique_ptr<presence::CredentialStorageInitializer>
      presence_credential_storage_initializer_;

  bool shut_down_ = false;

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
};

}  // namespace ash::nearby

#endif  // CHROME_BROWSER_ASH_NEARBY_NEARBY_DEPENDENCIES_PROVIDER_H_
