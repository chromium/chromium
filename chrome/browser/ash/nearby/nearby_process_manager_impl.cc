// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/nearby_process_manager_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/nearby/bluetooth_adapter_manager.h"
#include "chrome/browser/ash/nearby/nearby_dependencies_provider.h"
#include "chrome/browser/nearby_sharing/sharing_mojo_service.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "components/cross_device/logging/logging.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {
namespace nearby {
namespace {

NearbyProcessManagerImpl::Factory* g_test_factory = nullptr;

constexpr base::TimeDelta kProcessCleanupTimeout = base::Seconds(5);

void OnSharingShutDownComplete(
    mojo::Remote<sharing::mojom::Sharing> sharing,
    mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>
        connections,
    mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence> presence,
    mojo::SharedRemote<sharing::mojom::NearbySharingDecoder> decoder) {
  CD_LOG(INFO, Feature::NS) << "Asynchronous process shutdown complete.";
  // Note: Let the parameters go out of scope, which will disconnect them.
}

}  // namespace

// static
std::unique_ptr<NearbyProcessManager> NearbyProcessManagerImpl::Factory::Create(
    NearbyDependenciesProvider* nearby_dependencies_provider,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (g_test_factory) {
    return g_test_factory->BuildInstance(nearby_dependencies_provider,
                                         std::move(timer));
  }

  return base::WrapUnique(new NearbyProcessManagerImpl(
      nearby_dependencies_provider, std::move(timer),
      base::BindRepeating(&sharing::LaunchSharing)));
}

// static
void NearbyProcessManagerImpl::Factory::SetFactoryForTesting(Factory* factory) {
  g_test_factory = factory;
}

NearbyProcessManagerImpl::NearbyReferenceImpl::NearbyReferenceImpl(
    const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
        connections,
    const mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>&
        presence,
    const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>& decoder,
    const mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>&
        quick_start_decoder,
    base::OnceClosure destructor_callback)
    : connections_(connections),
      presence_(presence),
      decoder_(decoder),
      quick_start_decoder_(quick_start_decoder),
      destructor_callback_(std::move(destructor_callback)) {}

NearbyProcessManagerImpl::NearbyReferenceImpl::~NearbyReferenceImpl() {
  // Reset the SharedRemotes before the destructor callback is run to ensure
  // that all connections to the utility process are destroyed before we attempt
  // to tear the process down.
  connections_.reset();
  presence_.reset();
  decoder_.reset();

  std::move(destructor_callback_).Run();
}

const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
NearbyProcessManagerImpl::NearbyReferenceImpl::GetNearbyConnections() const {
  return connections_;
}

const mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>&
NearbyProcessManagerImpl::NearbyReferenceImpl::GetNearbyPresence() const {
  return presence_;
}

const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>&
NearbyProcessManagerImpl::NearbyReferenceImpl::GetNearbySharingDecoder() const {
  return decoder_;
}

const mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>&
NearbyProcessManagerImpl::NearbyReferenceImpl::GetQuickStartDecoder() const {
  return quick_start_decoder_;
}

NearbyProcessManagerImpl::NearbyProcessManagerImpl(
    NearbyDependenciesProvider* nearby_dependencies_provider,
    std::unique_ptr<base::OneShotTimer> timer,
    const base::RepeatingCallback<
        mojo::PendingRemote<sharing::mojom::Sharing>()>& sharing_binder)
    : nearby_dependencies_provider_(nearby_dependencies_provider),
      shutdown_debounce_timer_(std::move(timer)),
      sharing_binder_(sharing_binder) {}

NearbyProcessManagerImpl::~NearbyProcessManagerImpl() = default;

std::unique_ptr<NearbyProcessManager::NearbyProcessReference>
NearbyProcessManagerImpl::GetNearbyProcessReference(
    NearbyProcessStoppedCallback on_process_stopped_callback) {
  if (shut_down_) {
    return nullptr;
  }

  if (!sharing_ || !connections_ || !presence_ || !decoder_) {
    if (!AttemptToBindToUtilityProcess()) {
      CD_LOG(WARNING, Feature::NS)
          << "Could not connect to Nearby utility process; this "
          << "likely means that the attempt was during shutdown.";
      return nullptr;
    }
  }

  CD_LOG(VERBOSE, Feature::NS) << "New Nearby process reference requested.";
  auto reference_id = base::UnguessableToken::Create();
  id_to_process_stopped_callback_map_.emplace(
      reference_id, std::move(on_process_stopped_callback));

  // If we were waiting to shut down the process but a new client was added,
  // stop the timer since the process needs to be kept alive for this new
  // client.
  shutdown_debounce_timer_->Stop();

  return std::make_unique<NearbyReferenceImpl>(
      connections_, presence_, decoder_, quick_start_decoder_,
      base::BindOnce(&NearbyProcessManagerImpl::OnReferenceDeleted,
                     weak_ptr_factory_.GetWeakPtr(), reference_id));
}

void NearbyProcessManagerImpl::ShutDownProcess() {
  DoShutDownProcess(NearbyProcessShutdownReason::kNormal);
}

void NearbyProcessManagerImpl::Shutdown() {
  if (shut_down_) {
    return;
  }

  shut_down_ = true;

  NearbyProcessShutdownReason shutdown_reason =
      NearbyProcessShutdownReason::kNormal;

  DoShutDownProcess(shutdown_reason);
  NotifyProcessStopped(shutdown_reason);
}

bool NearbyProcessManagerImpl::AttemptToBindToUtilityProcess() {
  CHECK(!sharing_ && !connections_ && !presence_ && !decoder_);

  sharing::mojom::NearbyDependenciesPtr deps =
      nearby_dependencies_provider_->GetDependencies();

  if (!deps) {
    return false;
  }

  CD_LOG(INFO, Feature::NS) << "Starting up Nearby utility process.";

  // Bind to the Sharing interface, which launches the process.
  sharing_.Bind(sharing_binder_.Run());
  sharing_.set_disconnect_handler(
      base::BindOnce(&NearbyProcessManagerImpl::OnSharingProcessCrash,
                     weak_ptr_factory_.GetWeakPtr()));

  // Remotes for NearbyConnections and NearbySharingDecoder are bound on the
  // calling sequence by providing a null |bind_task_runner|.
  mojo::PendingRemote<::nearby::connections::mojom::NearbyConnections>
      connections;
  mojo::PendingReceiver<::nearby::connections::mojom::NearbyConnections>
      connections_receiver = connections.InitWithNewPipeAndPassReceiver();
  connections_.Bind(std::move(connections), /*bind_task_runner=*/nullptr);
  connections_.set_disconnect_handler(
      base::BindOnce(
          &NearbyProcessManagerImpl::OnMojoPipeDisconnect,
          weak_ptr_factory_.GetWeakPtr(),
          NearbyProcessShutdownReason::kConnectionsMojoPipeDisconnection),
      base::SequencedTaskRunner::GetCurrentDefault());

  mojo::PendingRemote<::ash::nearby::presence::mojom::NearbyPresence> presence;
  mojo::PendingReceiver<::ash::nearby::presence::mojom::NearbyPresence>
      presence_receiver = presence.InitWithNewPipeAndPassReceiver();
  presence_.Bind(std::move(presence), /*bind_task_runner=*/nullptr);
  presence_.set_disconnect_handler(
      base::BindOnce(
          &NearbyProcessManagerImpl::OnMojoPipeDisconnect,
          weak_ptr_factory_.GetWeakPtr(),
          NearbyProcessShutdownReason::kPresenceMojoPipeDisconnection),
      base::SequencedTaskRunner::GetCurrentDefault());

  mojo::PendingRemote<sharing::mojom::NearbySharingDecoder> decoder;
  mojo::PendingReceiver<sharing::mojom::NearbySharingDecoder> decoder_receiver =
      decoder.InitWithNewPipeAndPassReceiver();
  decoder_.Bind(std::move(decoder), /*bind_task_runner=*/nullptr);
  decoder_.set_disconnect_handler(
      base::BindOnce(
          &NearbyProcessManagerImpl::OnMojoPipeDisconnect,
          weak_ptr_factory_.GetWeakPtr(),
          NearbyProcessShutdownReason::kDecoderMojoPipeDisconnection),
      base::SequencedTaskRunner::GetCurrentDefault());

  mojo::PendingRemote<quick_start::mojom::QuickStartDecoder>
      quick_start_decoder;
  mojo::PendingReceiver<quick_start::mojom::QuickStartDecoder>
      quick_start_decoder_receiver =
          quick_start_decoder.InitWithNewPipeAndPassReceiver();
  quick_start_decoder_.Bind(std::move(quick_start_decoder),
                            /*bind_task_runner=*/nullptr);
  quick_start_decoder_.set_disconnect_handler(
      base::BindOnce(
          &NearbyProcessManagerImpl::OnMojoPipeDisconnect,
          weak_ptr_factory_.GetWeakPtr(),
          NearbyProcessShutdownReason::kDecoderMojoPipeDisconnection),
      base::SequencedTaskRunner::GetCurrentDefault());

  // Pass these references to Connect() to start up the process.
  sharing_->Connect(std::move(deps), std::move(connections_receiver),
                    std::move(presence_receiver), std::move(decoder_receiver),
                    std::move(quick_start_decoder_receiver));

  return true;
}

void NearbyProcessManagerImpl::OnSharingProcessCrash() {
  CD_LOG(ERROR, Feature::NS) << "The utility process has crashed.";

  NearbyProcessShutdownReason shutdown_reason =
      NearbyProcessShutdownReason::kCrash;

  DoShutDownProcess(shutdown_reason);
  NotifyProcessStopped(shutdown_reason);
}

void NearbyProcessManagerImpl::OnMojoPipeDisconnect(
    NearbyProcessShutdownReason shutdown_reason) {
  CD_LOG(ERROR, Feature::NS)
      << "The browser process has detected that the utility process "
         "disconnected from a mojo pipe. ["
      << shutdown_reason << "]";

  DoShutDownProcess(shutdown_reason);
  NotifyProcessStopped(shutdown_reason);
}

void NearbyProcessManagerImpl::OnReferenceDeleted(
    const base::UnguessableToken& reference_id) {
  auto it = id_to_process_stopped_callback_map_.find(reference_id);
  DCHECK(it != id_to_process_stopped_callback_map_.end());

  // Do not call the callback because its owner has already explicitly deleted
  // its reference.
  id_to_process_stopped_callback_map_.erase(it);

  // If there are still active references, the process should be kept alive, so
  // return early.
  if (!id_to_process_stopped_callback_map_.empty()) {
    return;
  }

  CD_LOG(VERBOSE, Feature::NS)
      << "All Nearby references have been released; will shut down "
      << "process in " << kProcessCleanupTimeout << " unless a new "
      << "reference is obtained.";

  // Stop the process, but wait |kProcessCleanupTimeout| before doing so. Adding
  // this additional timeout works around issues during Nearby shutdown
  // (see https://crbug.com/1152609).
  // TODO(https://crbug.com/1152892): Remove this timeout.
  shutdown_debounce_timer_->Start(
      FROM_HERE, kProcessCleanupTimeout,
      base::BindOnce(&NearbyProcessManagerImpl::DoShutDownProcess,
                     weak_ptr_factory_.GetWeakPtr(),
                     NearbyProcessShutdownReason::kNormal));
}

void NearbyProcessManagerImpl::DoShutDownProcess(
    NearbyProcessShutdownReason shutdown_reason) {
  if (!sharing_ && !connections_ && !decoder_) {
    return;
  }

  // Ensure that we don't try to stop the process again.
  shutdown_debounce_timer_->Stop();

  CD_LOG(INFO, Feature::NS) << "Shutting down Nearby utility process.";

  base::UmaHistogramEnumeration(
      "Nearby.Connections.UtilityProcessShutdownReason", shutdown_reason);

  // Prevent the Remotes' disconnect handler callbacks from firing.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (!sharing_) {
    sharing_.reset();
    connections_.reset();
    presence_.reset();
    decoder_.reset();
    return;
  }

  // Start the asynchronous shutdown flow, and pass ownership of the existing
  // Remote and SharedRemotes to the callback. These instance fields will stay
  // alive until ShutDown() is complete, at which time they will go out of
  // scope and become disconnected in OnSharingShutDownComplete().
  sharing::mojom::Sharing* sharing = sharing_.get();
  sharing->ShutDown(base::BindOnce(&OnSharingShutDownComplete,
                                   std::move(sharing_), std::move(connections_),
                                   std::move(presence_), std::move(decoder_)));
  nearby_dependencies_provider_->PrepareForShutdown();
}

void NearbyProcessManagerImpl::NotifyProcessStopped(
    NearbyProcessShutdownReason shutdown_reason) {
  // We are notifying clients that references are no longer active, so
  // invalidate WeakPtrs so that OnReferenceDeleted() is not invoked when
  // clients respond to the callback by releasing their references.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Move the map to a local variable to ensure that the instance field is
  // empty before any callbacks are made.
  auto old_map = std::move(id_to_process_stopped_callback_map_);
  id_to_process_stopped_callback_map_.clear();

  // Invoke the "process stopped" callback for each client.
  for (auto& it : old_map) {
    std::move(it.second).Run(shutdown_reason);
  }
}

}  // namespace nearby
}  // namespace ash
