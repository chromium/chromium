// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_process_manager.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/webrtc_signaling_messenger.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sharing/webrtc/ice_config_fetcher.h"
#include "chrome/browser/sharing/webrtc/sharing_mojo_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/services/sharing/public/mojom/nearby_connections.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/network_context_client_base.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "device/bluetooth/adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"

namespace {

ProfileAttributesEntry* GetStoredNearbyProfile() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return nullptr;

  base::FilePath advertising_profile_path =
      local_state->GetFilePath(::prefs::kNearbySharingActiveProfilePrefName);
  if (advertising_profile_path.empty())
    return nullptr;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  ProfileAttributesEntry* entry;
  if (!storage.GetProfileAttributesWithPath(advertising_profile_path, &entry)) {
    // Stored profile path is invalid so remove it.
    local_state->ClearPref(::prefs::kNearbySharingActiveProfilePrefName);
    return nullptr;
  }
  return entry;
}

void SetStoredNearbyProfile(Profile* profile) {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return;

  if (profile) {
    local_state->SetFilePath(::prefs::kNearbySharingActiveProfilePrefName,
                             profile->GetPath());
  } else {
    local_state->ClearPref(::prefs::kNearbySharingActiveProfilePrefName);
  }
}

bool IsStoredNearbyProfile(Profile* profile) {
  ProfileAttributesEntry* entry = GetStoredNearbyProfile();
  if (!entry)
    return profile == nullptr;
  return profile && entry->GetPath() == profile->GetPath();
}

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

// static
NearbyProcessManager& NearbyProcessManager::GetInstance() {
  static base::NoDestructor<NearbyProcessManager> instance;
  return *instance;
}

void NearbyProcessManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyProcessManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ProfileAttributesEntry* NearbyProcessManager::GetActiveProfile() const {
  return GetStoredNearbyProfile();
}

bool NearbyProcessManager::IsActiveProfile(Profile* profile) const {
  // If the active profile is not loaded yet, try looking in prefs.
  if (!active_profile_)
    return IsStoredNearbyProfile(profile);

  return active_profile_ == profile;
}

bool NearbyProcessManager::IsAnyProfileActive() const {
  return !IsActiveProfile(/*profile=*/nullptr);
}

void NearbyProcessManager::SetActiveProfile(Profile* profile) {
  if (IsActiveProfile(profile))
    return;

  active_profile_ = profile;
  SetStoredNearbyProfile(active_profile_);
  StopProcess(active_profile_);

  for (auto& observer : observers_)
    observer.OnNearbyProfileChanged(profile);
}

void NearbyProcessManager::ClearActiveProfile() {
  SetActiveProfile(/*profile=*/nullptr);
}

location::nearby::connections::mojom::NearbyConnections*
NearbyProcessManager::GetOrStartNearbyConnections(Profile* profile) {
  if (!IsActiveProfile(profile))
    return nullptr;

  active_profile_ = profile;
  // Launch a new Nearby Connections interface if required.
  if (!connections_.is_bound())
    BindNearbyConnections();

  return connections_.get();
}

sharing::mojom::NearbySharingDecoder*
NearbyProcessManager::GetOrStartNearbySharingDecoder(Profile* profile) {
  if (!IsActiveProfile(profile))
    return nullptr;

  active_profile_ = profile;
  // Launch a new Nearby Sharing Decoder interface if required.
  if (!decoder_.is_bound())
    BindNearbySharingDecoder();

  return decoder_.get();
}

void NearbyProcessManager::StopProcess(Profile* profile) {
  if (!IsActiveProfile(profile))
    return;

  bool was_running = sharing_process_.is_bound();

  connections_.reset();
  decoder_.reset();
  sharing_process_.reset();

  if (was_running) {
    for (auto& observer : observers_)
      observer.OnNearbyProcessStopped();
  }
}

void NearbyProcessManager::OnProfileAdded(Profile* profile) {
  // Cache active |profile| once it loads so we don't have to check prefs.
  if (IsActiveProfile(profile))
    active_profile_ = profile;
}

void NearbyProcessManager::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {
  if (IsActiveProfile(profile))
    SetActiveProfile(nullptr);
}

void NearbyProcessManager::BindSharingProcess(
    mojo::PendingRemote<sharing::mojom::Sharing> sharing) {
  sharing_process_.Bind(std::move(sharing));
  // base::Unretained() is safe as |this| is a singleton.
  sharing_process_.set_disconnect_handler(base::BindOnce(
      &NearbyProcessManager::OnNearbyProcessStopped, base::Unretained(this)));
}

NearbyProcessManager::NearbyProcessManager() {
  // profile_manager() might be null in tests or during shutdown.
  if (auto* manager = g_browser_process->profile_manager())
    manager->AddObserver(this);
}

NearbyProcessManager::~NearbyProcessManager() {
  if (auto* manager = g_browser_process->profile_manager())
    manager->RemoveObserver(this);
}

void NearbyProcessManager::LaunchNewProcess() {
  // Stop any running process and mojo pipes.
  StopProcess(active_profile_);

  // Launch a new sandboxed process.
  // TODO(crbug.com/1095650): Set process name to "Nearby Sharing".
  BindSharingProcess(sharing::LaunchSharing());
}

void NearbyProcessManager::BindNearbyConnections() {
  // Start a new process if there is none running yet.
  if (!sharing_process_.is_bound())
    LaunchNewProcess();

  mojo::PendingReceiver<NearbyConnectionsMojom> pending_receiver =
      connections_.BindNewPipeAndPassReceiver();
  auto dependencies = location::nearby::connections::mojom::
      NearbyConnectionsDependencies::New();
  location::nearby::connections::mojom::NearbyConnectionsDependencies*
      dependencies_ptr = dependencies.get();

  // base::Unretained() is safe as |this| is a singleton.
  auto done_closure = base::BarrierClosure(
      /*num_closures=*/2,
      base::BindOnce(&NearbyProcessManager::OnDependenciesGathered,
                     base::Unretained(this), std::move(pending_receiver),
                     std::move(dependencies)));

  GetBluetoothAdapter(dependencies_ptr,
                      base::ScopedClosureRunner(done_closure));

  GetWebRtcDependencies(dependencies_ptr,
                        base::ScopedClosureRunner(done_closure));

  // Terminate the process if the Nearby Connections interface disconnects as
  // that indicated an incorrect state and we have to restart the process.
  // base::Unretained() is safe as |this| is a singleton.
  connections_.set_disconnect_handler(base::BindOnce(
      &NearbyProcessManager::OnNearbyProcessStopped, base::Unretained(this)));
}

void NearbyProcessManager::GetBluetoothAdapter(
    location::nearby::connections::mojom::NearbyConnectionsDependencies*
        dependencies,
    base::ScopedClosureRunner done_closure) {
  NS_LOG(VERBOSE) << __func__
                  << " Request for Bluetooth "
                     "adapter received on the browser process.";
  if (!device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    NS_LOG(VERBOSE) << __func__ << " Bluetooth is not supported on this device";
    dependencies->bluetooth_adapter = mojo::NullRemote();
    return;
  }

  // base::Unretained() is safe as |this| is a singleton.
  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &NearbyProcessManager::OnGetBluetoothAdapter, base::Unretained(this),
      dependencies, std::move(done_closure)));
}

void NearbyProcessManager::OnGetBluetoothAdapter(
    location::nearby::connections::mojom::NearbyConnectionsDependencies*
        dependencies,
    base::ScopedClosureRunner done_closure,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter->IsPresent()) {
    NS_LOG(VERBOSE) << __func__ << " Bluetooth adapter is not present";
    dependencies->bluetooth_adapter = mojo::NullRemote();
    return;
  }

  mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;
  mojo::MakeSelfOwnedReceiver(std::make_unique<bluetooth::Adapter>(adapter),
                              pending_adapter.InitWithNewPipeAndPassReceiver());

  NS_LOG(VERBOSE) << __func__ << " Got bluetooth adapter";
  dependencies->bluetooth_adapter = std::move(pending_adapter);
}

void NearbyProcessManager::GetWebRtcDependencies(
    location::nearby::connections::mojom::NearbyConnectionsDependencies*
        dependencies,
    base::ScopedClosureRunner done_closure) {
  DCHECK(active_profile_);

  auto* network_context =
      content::BrowserContext::GetDefaultStoragePartition(active_profile_)
          ->GetNetworkContext();

  auto url_loader_factory = active_profile_->GetURLLoaderFactory();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(active_profile_);

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
  MojoPipe<sharing::mojom::IceConfigFetcher> ice_config_fetcher;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<IceConfigFetcher>(url_loader_factory),
      std::move(ice_config_fetcher.receiver));

  MojoPipe<sharing::mojom::WebRtcSignalingMessenger> messenger;
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebRtcSignalingMessenger>(
                                  identity_manager, url_loader_factory),
                              std::move(messenger.receiver));

  dependencies->webrtc_dependencies =
      location::nearby::connections::mojom::WebRtcDependencies::New(
          std::move(socket_manager.remote), std::move(mdns_responder.remote),
          std::move(ice_config_fetcher.remote), std::move(messenger.remote));
}

void NearbyProcessManager::OnDependenciesGathered(
    mojo::PendingReceiver<NearbyConnectionsMojom> receiver,
    location::nearby::connections::mojom::NearbyConnectionsDependenciesPtr
        dependencies) {
  if (!sharing_process_.is_bound())
    return;

  // Create the Nearby Connections stack in the sandboxed process.
  // base::Unretained() calls below are safe as |this| is a singleton.
  sharing_process_->CreateNearbyConnections(
      std::move(dependencies),
      base::BindOnce(&NearbyProcessManager::OnNearbyConnections,
                     base::Unretained(this), std::move(receiver)));
}

void NearbyProcessManager::OnNearbyConnections(
    mojo::PendingReceiver<NearbyConnectionsMojom> receiver,
    mojo::PendingRemote<NearbyConnectionsMojom> remote) {
  if (!mojo::FusePipes(std::move(receiver), std::move(remote))) {
    NS_LOG(WARNING) << "Failed to initialize Nearby Connections process";
    StopProcess(active_profile_);
    return;
  }

  for (auto& observer : observers_)
    observer.OnNearbyProcessStarted();
}

void NearbyProcessManager::OnNearbyProcessStopped() {
  StopProcess(active_profile_);
}

void NearbyProcessManager::BindNearbySharingDecoder() {
  // Start a new process if there is none running yet.
  if (!sharing_process_.is_bound())
    LaunchNewProcess();

  // Create the Nearby Sharing Decoder stack in the sandboxed process.
  // base::Unretained() calls below are safe as |this| is a singleton.
  sharing_process_->CreateNearbySharingDecoder(base::BindOnce(
      &NearbyProcessManager::OnNearbySharingDecoder, base::Unretained(this),
      decoder_.BindNewPipeAndPassReceiver()));

  // Terminate the process if the Nearby Sharing Decoder interface disconnects
  // as that indicated an incorrect state and we have to restart the process.
  decoder_.set_disconnect_handler(base::BindOnce(
      &NearbyProcessManager::OnNearbyProcessStopped, base::Unretained(this)));
}

void NearbyProcessManager::OnNearbySharingDecoder(
    mojo::PendingReceiver<NearbySharingDecoderMojom> receiver,
    mojo::PendingRemote<NearbySharingDecoderMojom> remote) {
  if (!mojo::FusePipes(std::move(receiver), std::move(remote))) {
    NS_LOG(WARNING) << "Failed to initialize Nearby Sharing Decoder process";
    StopProcess(active_profile_);
    return;
  }

  for (auto& observer : observers_)
    observer.OnNearbyProcessStarted();
}
