// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/nearby_dependencies_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/network_config_service.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ash/nearby/bluetooth_adapter_manager.h"
#include "chrome/browser/ash/nearby/nearby_dependencies_provider_factory.h"
#include "chrome/browser/ash/nearby/presence/credential_storage/credential_storage_initializer.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_switches.h"
#include "chrome/browser/nearby_sharing/firewall_hole/nearby_connections_firewall_hole_factory.h"
#include "chrome/browser/nearby_sharing/mdns/nearby_connections_mdns_manager.h"
#include "chrome/browser/nearby_sharing/sharing_mojo_service.h"
#include "chrome/browser/nearby_sharing/tachyon_ice_config_fetcher.h"
#include "chrome/browser/nearby_sharing/tcp_socket/nearby_connections_tcp_socket_factory.h"
#include "chrome/browser/nearby_sharing/webrtc_signaling_messenger.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/mdns.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/tcp_socket_factory.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/cross_device/nearby/nearby_features.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"

namespace ash {
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

// Allows observers to be notified when NearbyDependenciesProvider is shut down.
class NearbyDependenciesProviderShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static NearbyDependenciesProviderShutdownNotifierFactory* GetInstance() {
    return base::Singleton<
        NearbyDependenciesProviderShutdownNotifierFactory>::get();
  }

  NearbyDependenciesProviderShutdownNotifierFactory(
      const NearbyDependenciesProviderShutdownNotifierFactory&) = delete;
  NearbyDependenciesProviderShutdownNotifierFactory& operator=(
      const NearbyDependenciesProviderShutdownNotifierFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      NearbyDependenciesProviderShutdownNotifierFactory>;

  NearbyDependenciesProviderShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "NearbyDependenciesProvider") {
    DependsOn(NearbyDependenciesProviderFactory::GetInstance());
  }
  ~NearbyDependenciesProviderShutdownNotifierFactory() override = default;
};

class MdnsResponderFactory : public ::sharing::mojom::MdnsResponderFactory {
 public:
  explicit MdnsResponderFactory(Profile* profile) : profile_(profile) {
    // Subscribe to be notified when NearbyDependenciesProvider shuts down.
    // NearbyDependenciesProvider is a KeyedService bound to |profile_|, so the
    // destruction of NearbyDependenciesProvider means that |profile_| is
    // invalid. This lets us return early from CreateMdnsResponder() to avoid
    // the risk of using |profile_| after it has been destroyed. Using
    // base::Unretained() is safe here because the MdnsResponderFactory is
    // guaranteed to outlive |shutdown_subscription_|.
    shutdown_subscription_ =
        NearbyDependenciesProviderShutdownNotifierFactory::GetInstance()
            ->Get(profile_)
            ->Subscribe(base::BindRepeating(&MdnsResponderFactory::Shutdown,
                                            base::Unretained(this)));
  }

  void CreateMdnsResponder(mojo::PendingReceiver<network::mojom::MdnsResponder>
                               responder_receiver) override {
    if (is_shutdown_) {
      return;
    }

    auto* partition = profile_->GetDefaultStoragePartition();
    if (!partition) {
      LOG(ERROR) << "MdnsResponderFactory::" << __func__
                 << ": GetDefaultStoragePartition(profile) failed.";
      // When |responder_receiver| goes out of scope the pipe will disconnect.
      return;
    }

    auto* network_context = partition->GetNetworkContext();
    if (!network_context) {
      LOG(ERROR) << "MdnsResponderFactory::" << __func__
                 << ": GetNetworkContext() failed.";
      // When |responder_receiver| goes out of scope the pipe will disconnect.
      return;
    }

    network_context->CreateMdnsResponder(std::move(responder_receiver));
  }

 private:
  void Shutdown() { is_shutdown_ = true; }

  bool is_shutdown_ = false;
  raw_ptr<Profile, LeakedDanglingUntriaged> profile_;
  base::CallbackListSubscription shutdown_subscription_;
};

}  // namespace

NearbyDependenciesProvider::NearbyDependenciesProvider(
    Profile* profile,
    signin::IdentityManager* identity_manager)
    : profile_(profile), identity_manager_(identity_manager) {
  DCHECK(profile_);
  bluetooth_manager_ = std::make_unique<BluetoothAdapterManager>();
}

NearbyDependenciesProvider::NearbyDependenciesProvider() = default;

NearbyDependenciesProvider::~NearbyDependenciesProvider() = default;

::sharing::mojom::NearbyDependenciesPtr
NearbyDependenciesProvider::GetDependencies() {
  if (shut_down_) {
    return nullptr;
  }

  auto dependencies = ::sharing::mojom::NearbyDependencies::New();

  if (device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    dependencies->bluetooth_adapter = GetBluetoothAdapterPendingRemote();
  } else {
    dependencies->bluetooth_adapter = mojo::NullRemote();
  }

  // TOOD(b/317307931): Re-visit security considerations before enabling
  // this feature by default/ramping up via Finch.
  if (ash::features::IsNearbyPresenceEnabled()) {
    dependencies->nearby_presence_credential_storage =
        GetNearbyPresenceCredentialStoragePendingRemote();
  } else {
    dependencies->nearby_presence_credential_storage = mojo::NullRemote();
  }

  dependencies->webrtc_dependencies = GetWebRtcDependencies();
  dependencies->wifilan_dependencies = GetWifiLanDependencies();
  dependencies->wifidirect_dependencies = GetWifiDirectDependencies();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNearbyShareVerboseLogging)) {
    dependencies->min_log_severity =
        ::nearby::api::LogMessage::Severity::kVerbose;
  }

  return dependencies;
}

void NearbyDependenciesProvider::PrepareForShutdown() {
  if (bluetooth_manager_) {
    bluetooth_manager_->Shutdown();
  }

  presence_credential_storage_initializer_.reset();
}

void NearbyDependenciesProvider::Shutdown() {
  shut_down_ = true;
}

mojo::PendingRemote<bluetooth::mojom::Adapter>
NearbyDependenciesProvider::GetBluetoothAdapterPendingRemote() {
  mojo::PendingReceiver<bluetooth::mojom::Adapter> pending_receiver;
  mojo::PendingRemote<bluetooth::mojom::Adapter> pending_remote =
      pending_receiver.InitWithNewPipeAndPassRemote();
  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &BluetoothAdapterManager::Initialize, bluetooth_manager_->GetWeakPtr(),
      std::move(pending_receiver)));
  return pending_remote;
}

mojo::PendingRemote<presence::mojom::NearbyPresenceCredentialStorage>
NearbyDependenciesProvider::GetNearbyPresenceCredentialStoragePendingRemote() {
  mojo::PendingReceiver<presence::mojom::NearbyPresenceCredentialStorage>
      pending_receiver;
  mojo::PendingRemote<presence::mojom::NearbyPresenceCredentialStorage>
      pending_remote = pending_receiver.InitWithNewPipeAndPassRemote();

  presence_credential_storage_initializer_ =
      std::make_unique<presence::CredentialStorageInitializer>(
          std::move(pending_receiver), profile_);
  presence_credential_storage_initializer_->Initialize();

  return pending_remote;
}

::sharing::mojom::WebRtcDependenciesPtr
NearbyDependenciesProvider::GetWebRtcDependencies() {
  MojoPipe<network::mojom::P2PTrustedSocketManagerClient> socket_manager_client;
  MojoPipe<network::mojom::P2PTrustedSocketManager> trusted_socket_manager;
  MojoPipe<network::mojom::P2PSocketManager> socket_manager;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<P2PTrustedSocketManagerClientImpl>(
          std::move(trusted_socket_manager.remote)),
      std::move(socket_manager_client.receiver));

  // Create socket manager.
  GetNetworkContext()->CreateP2PSocketManager(
      net::NetworkAnonymizationKey::CreateTransient(),
      std::move(socket_manager_client.remote),
      std::move(trusted_socket_manager.receiver),
      std::move(socket_manager.receiver));

  MojoPipe<::sharing::mojom::MdnsResponderFactory> mdns_responder_factory_pipe;
  mojo::MakeSelfOwnedReceiver(std::make_unique<MdnsResponderFactory>(profile_),
                              std::move(mdns_responder_factory_pipe.receiver));

  // Create ice config fetcher.
  auto url_loader_factory = profile_->GetURLLoaderFactory();
  MojoPipe<::sharing::mojom::IceConfigFetcher> ice_config_fetcher;
  mojo::MakeSelfOwnedReceiver(std::make_unique<TachyonIceConfigFetcher>(
                                  identity_manager_, url_loader_factory),
                              std::move(ice_config_fetcher.receiver));

  MojoPipe<::sharing::mojom::WebRtcSignalingMessenger> messenger;
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebRtcSignalingMessenger>(
                                  identity_manager_, url_loader_factory),
                              std::move(messenger.receiver));

  return ::sharing::mojom::WebRtcDependencies::New(
      std::move(socket_manager.remote),
      std::move(mdns_responder_factory_pipe.remote),
      std::move(ice_config_fetcher.remote), std::move(messenger.remote));
}

::sharing::mojom::WifiLanDependenciesPtr
NearbyDependenciesProvider::GetWifiLanDependencies() {
  if (!base::FeatureList::IsEnabled(::features::kNearbySharingWifiLan)) {
    return nullptr;
  }

  MojoPipe<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config;
  ash::GetNetworkConfigService(std::move(cros_network_config.receiver));

  MojoPipe<::sharing::mojom::FirewallHoleFactory> firewall_hole_factory;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NearbyConnectionsFirewallHoleFactory>(),
      std::move(firewall_hole_factory.receiver));

  MojoPipe<::sharing::mojom::TcpSocketFactory> tcp_socket_factory;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NearbyConnectionsTcpSocketFactory>(
          base::BindRepeating(&NearbyDependenciesProvider::GetNetworkContext,
                              base::Unretained(this))),
      std::move(tcp_socket_factory.receiver));

  MojoPipe<::sharing::mojom::MdnsManager> mdns_manager;
  if (::features::IsNearbyMdnsEnabled()) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<::nearby::sharing::NearbyConnectionsMdnsManager>(),
        std::move(mdns_manager.receiver));
  }

  return ::sharing::mojom::WifiLanDependencies::New(
      std::move(cros_network_config.remote),
      std::move(firewall_hole_factory.remote),
      std::move(tcp_socket_factory.remote),
      (::features::IsNearbyMdnsEnabled() ? std::move(mdns_manager.remote)
                                         : mojo::NullRemote()));
}

sharing::mojom::WifiDirectDependenciesPtr
NearbyDependenciesProvider::GetWifiDirectDependencies() {
  if (!ash::features::IsWifiDirectEnabled() ||
      !base::FeatureList::IsEnabled(::features::kNearbySharingWifiDirect)) {
    return nullptr;
  }

  MojoPipe<sharing::mojom::FirewallHoleFactory> firewall_hole_factory;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NearbyConnectionsFirewallHoleFactory>(),
      std::move(firewall_hole_factory.receiver));

  wifi_direct_manager_ = std::make_unique<wifi_direct::WifiDirectManager>();
  MojoPipe<wifi_direct::mojom::WifiDirectManager> manager;
  wifi_direct_manager_->BindPendingReceiver(std::move(manager.receiver));

  return sharing::mojom::WifiDirectDependencies::New(
      std::move(manager.remote), std::move(firewall_hole_factory.remote));
}

network::mojom::NetworkContext*
NearbyDependenciesProvider::GetNetworkContext() {
  return profile_->GetDefaultStoragePartition()->GetNetworkContext();
}

// static
void NearbyDependenciesProvider::EnsureFactoryBuilt() {
  NearbyDependenciesProviderShutdownNotifierFactory::GetInstance();
}

}  // namespace nearby
}  // namespace ash
