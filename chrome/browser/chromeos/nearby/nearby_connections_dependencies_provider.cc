// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/nearby/nearby_connections_dependencies_provider.h"

#include "chrome/browser/chromeos/nearby/bluetooth_adapter_util.h"
#include "chrome/browser/nearby_sharing/webrtc_signaling_messenger.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/webrtc/ice_config_fetcher.h"
#include "chrome/browser/sharing/webrtc/sharing_mojo_service.h"
#include "content/public/browser/storage_partition.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"

namespace chromeos {
namespace nearby {
namespace {

template <typename T>
struct MojoPipe {
  mojo::PendingRemote<T> remote;
  mojo::PendingReceiver<T> receiver{remote.InitWithNewPipeAndPassReceiver()};
};

class P2PTrustedSocketManagerClientImpl
    : public network::mojom::P2PTrustedSocketManagerClient {
 public:
  explicit P2PTrustedSocketManagerClientImpl(
      mojo::PendingRemote<network::mojom::P2PTrustedSocketManager>
          socket_manager)
      : socket_manager_(std::move(socket_manager)) {}
  ~P2PTrustedSocketManagerClientImpl() override = default;

  // network::mojom::P2PTrustedSocketManagerClient:
  void InvalidSocketPortRangeRequested() override { NOTIMPLEMENTED(); }
  void DumpPacket(const std::vector<uint8_t>& packet_header,
                  uint64_t packet_length,
                  bool incoming) override {
    NOTIMPLEMENTED();
  }

 private:
  mojo::Remote<network::mojom::P2PTrustedSocketManager> socket_manager_;
};

}  // namespace

NearbyConnectionsDependenciesProvider::NearbyConnectionsDependenciesProvider(
    Profile* profile,
    signin::IdentityManager* identity_manager)
    : profile_(profile), identity_manager_(identity_manager) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
}

NearbyConnectionsDependenciesProvider::NearbyConnectionsDependenciesProvider() =
    default;

NearbyConnectionsDependenciesProvider::
    ~NearbyConnectionsDependenciesProvider() = default;

location::nearby::connections::mojom::NearbyConnectionsDependenciesPtr
NearbyConnectionsDependenciesProvider::GetDependencies() {
  if (shut_down_)
    return nullptr;

  auto dependencies = location::nearby::connections::mojom::
      NearbyConnectionsDependencies::New();

  if (device::BluetoothAdapterFactory::IsBluetoothSupported())
    dependencies->bluetooth_adapter = GetBluetoothAdapterPendingRemote();
  else
    dependencies->bluetooth_adapter = mojo::NullRemote();

  dependencies->webrtc_dependencies = GetWebRtcDependencies();

  return dependencies;
}

void NearbyConnectionsDependenciesProvider::Shutdown() {
  shut_down_ = true;
}

mojo::PendingRemote<bluetooth::mojom::Adapter>
NearbyConnectionsDependenciesProvider::GetBluetoothAdapterPendingRemote() {
  mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&MakeSelfOwnedBluetoothAdapter,
                     pending_adapter.InitWithNewPipeAndPassReceiver()));

  return pending_adapter;
}

location::nearby::connections::mojom::WebRtcDependenciesPtr
NearbyConnectionsDependenciesProvider::GetWebRtcDependencies() {
  auto* network_context =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetNetworkContext();

  MojoPipe<network::mojom::P2PTrustedSocketManagerClient> socket_manager_client;
  MojoPipe<network::mojom::P2PTrustedSocketManager> trusted_socket_manager;
  MojoPipe<network::mojom::P2PSocketManager> socket_manager;
  MojoPipe<network::mojom::MdnsResponder> mdns_responder;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<P2PTrustedSocketManagerClientImpl>(
          std::move(trusted_socket_manager.remote)),
      std::move(socket_manager_client.receiver));

  // Create socket manager.
  network_context->CreateP2PSocketManager(
      net::NetworkIsolationKey::CreateTransient(),
      std::move(socket_manager_client.remote),
      std::move(trusted_socket_manager.receiver),
      std::move(socket_manager.receiver));

  // Create mdns responder.
  network_context->CreateMdnsResponder(std::move(mdns_responder.receiver));

  // Create ice config fetcher.
  auto url_loader_factory = profile_->GetURLLoaderFactory();
  MojoPipe<sharing::mojom::IceConfigFetcher> ice_config_fetcher;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<IceConfigFetcher>(url_loader_factory),
      std::move(ice_config_fetcher.receiver));

  MojoPipe<sharing::mojom::WebRtcSignalingMessenger> messenger;
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebRtcSignalingMessenger>(
                                  identity_manager_, url_loader_factory),
                              std::move(messenger.receiver));

  return location::nearby::connections::mojom::WebRtcDependencies::New(
      std::move(socket_manager.remote), std::move(mdns_responder.remote),
      std::move(ice_config_fetcher.remote), std::move(messenger.remote));
}

}  // namespace nearby
}  // namespace chromeos
