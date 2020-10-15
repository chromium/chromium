// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/nearby/nearby_process_manager_impl.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/nearby/nearby_connections_dependencies_provider.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/sharing/webrtc/sharing_mojo_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {
namespace nearby {

NearbyProcessManagerImpl::NearbyReferenceImpl::NearbyReferenceImpl(
    const mojo::SharedRemote<
        location::nearby::connections::mojom::NearbyConnections>& connections,
    const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>& decoder,
    base::OnceClosure destructor_callback)
    : connections_(connections),
      decoder_(decoder),
      destructor_callback_(std::move(destructor_callback)) {}

NearbyProcessManagerImpl::NearbyReferenceImpl::~NearbyReferenceImpl() {
  // Reset the SharedRemotes before the destructor callback is run to ensure
  // that all connections to the utility process are destroyed before we attempt
  // to tear the process down.
  connections_.reset();
  decoder_.reset();

  std::move(destructor_callback_).Run();
}

const mojo::SharedRemote<
    location::nearby::connections::mojom::NearbyConnections>&
NearbyProcessManagerImpl::NearbyReferenceImpl::GetNearbyConnections() const {
  return connections_;
}

const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>&
NearbyProcessManagerImpl::NearbyReferenceImpl::GetNearbySharingDecoder() const {
  return decoder_;
}

NearbyProcessManagerImpl::NearbyProcessManagerImpl(
    NearbyConnectionsDependenciesProvider*
        nearby_connections_dependencies_provider)
    : nearby_connections_dependencies_provider_(
          nearby_connections_dependencies_provider) {}

NearbyProcessManagerImpl::~NearbyProcessManagerImpl() = default;

std::unique_ptr<NearbyProcessManager::NearbyProcessReference>
NearbyProcessManagerImpl::GetNearbyProcessReference(
    base::OnceClosure on_process_stopped_callback) {
  if (shut_down_)
    return nullptr;

  if (!sharing_.is_bound()) {
    bool bound_successfully = AttemptToBindToUtilityProcess();
    if (!bound_successfully) {
      NS_LOG(WARNING) << "Could not connect to Nearby utility process; this "
                      << "likely means that the attempt was during shutdown.";
      return nullptr;
    }
  }

  auto reference_id = base::UnguessableToken::Create();
  id_to_process_stopped_callback_map_.emplace(
      reference_id, std::move(on_process_stopped_callback));

  return std::make_unique<NearbyReferenceImpl>(
      connections_, decoder_,
      base::BindOnce(&NearbyProcessManagerImpl::OnReferenceDeleted,
                     weak_ptr_factory_.GetWeakPtr(), reference_id));
}

void NearbyProcessManagerImpl::Shutdown() {
  shut_down_ = true;

  if (!sharing_.is_bound())
    return;

  // Shut down process first, then notify existing clients of the shutdown via
  // OnSharingDisconnected().
  ShutDownProcess();
  OnSharingDisconnected();
}

bool NearbyProcessManagerImpl::AttemptToBindToUtilityProcess() {
  DCHECK(!sharing_.is_bound());

  location::nearby::connections::mojom::NearbyConnectionsDependenciesPtr deps =
      nearby_connections_dependencies_provider_->GetDependencies();

  if (!deps)
    return false;

  NS_LOG(INFO) << "Starting up Nearby utility process.";

  // Bind to the Sharing interface, which launches the process.
  sharing_.Bind(sharing::LaunchSharing());
  sharing_.set_disconnect_handler(
      base::BindOnce(&NearbyProcessManagerImpl::OnSharingDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));

  // Initialize a reference to NearbyConnections; bound on the calling sequence,
  // so null is passed during the Bind() call.
  // TODO(khorimoto): Change this code to pass a PendingReceiver instead of
  // receiving a PendingRemote.
  mojo::PendingRemote<location::nearby::connections::mojom::NearbyConnections>
      connections;
  mojo::PendingReceiver<location::nearby::connections::mojom::NearbyConnections>
      connections_receiver = connections.InitWithNewPipeAndPassReceiver();
  connections_.Bind(std::move(connections), /*bind_task_runner=*/nullptr);
  sharing_->CreateNearbyConnections(
      std::move(deps),
      base::BindOnce(&NearbyProcessManagerImpl::OnNearbyConnections,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(connections_receiver)));

  // Initialize a reference to NearbySharingDecoder; bound on the calling
  // sequence, so null is passed during the Bind() call.
  // TODO(khorimoto): Change this code to pass a PendingReceiver instead of
  // receiving a PendingRemote.
  mojo::PendingRemote<sharing::mojom::NearbySharingDecoder> decoder;
  mojo::PendingReceiver<sharing::mojom::NearbySharingDecoder> decoder_receiver =
      decoder.InitWithNewPipeAndPassReceiver();
  decoder_.Bind(std::move(decoder), /*bind_task_runner=*/nullptr);
  sharing_->CreateNearbySharingDecoder(base::BindOnce(
      &NearbyProcessManagerImpl::OnNearbySharingDecoder,
      weak_ptr_factory_.GetWeakPtr(), std::move(decoder_receiver)));

  return true;
}

void NearbyProcessManagerImpl::OnNearbyConnections(
    mojo::PendingReceiver<
        location::nearby::connections::mojom::NearbyConnections> receiver,
    mojo::PendingRemote<location::nearby::connections::mojom::NearbyConnections>
        connections) {
  mojo::FusePipes(std::move(receiver), std::move(connections));
}

void NearbyProcessManagerImpl::OnNearbySharingDecoder(
    mojo::PendingReceiver<sharing::mojom::NearbySharingDecoder> receiver,
    mojo::PendingRemote<sharing::mojom::NearbySharingDecoder> decoder) {
  mojo::FusePipes(std::move(receiver), std::move(decoder));
}

void NearbyProcessManagerImpl::OnSharingDisconnected() {
  NS_LOG(INFO) << "Nearby utility process has shut down.";
  sharing_.reset();

  // We are notifying clients that references are no longer active, so
  // invalidate WeakPtrs so that OnReferenceDeleted() is not invoked when
  // clients respond to the callback by releasing their references.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Move the map to a local variable to ensure that the instance field is
  // empty before any callbacks are made.
  base::flat_map<base::UnguessableToken, base::OnceClosure> old_map =
      std::move(id_to_process_stopped_callback_map_);
  id_to_process_stopped_callback_map_.clear();

  // Invoke the "process stopped" callback for each client.
  for (auto& it : old_map)
    std::move(it.second).Run();
}

void NearbyProcessManagerImpl::OnReferenceDeleted(
    const base::UnguessableToken& reference_id) {
  auto it = id_to_process_stopped_callback_map_.find(reference_id);
  DCHECK(it != id_to_process_stopped_callback_map_.end());
  id_to_process_stopped_callback_map_.erase(it);

  // If there are still active references, the process should be kept alive, so
  // return early.
  if (!id_to_process_stopped_callback_map_.empty())
    return;

  ShutDownProcess();
}

void NearbyProcessManagerImpl::ShutDownProcess() {
  NS_LOG(INFO) << "Shutting down Nearby utility process.";

  // Ensure that any in-progress CreateNearbyConnections() or
  // CreateNearbySharingDecoder() calls do not return.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // TODO(khorimoto): Use asynchronous shutdown flow.
  sharing_.reset();
  connections_.reset();
  decoder_.reset();
}

}  // namespace nearby
}  // namespace chromeos
