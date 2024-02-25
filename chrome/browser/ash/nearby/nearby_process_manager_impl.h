// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_NEARBY_PROCESS_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_NEARBY_NEARBY_PROCESS_MANAGER_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace ash {
namespace nearby {

class NearbyDependenciesProvider;

// NearbyProcessManager implementation, which is implemented as a KeyedService
// because its dependencies are associated with the current user.
//
// This class starts up the utility process when at least one client holds a
// NearbyReferenceImpl object. When the first client requests a reference, this
// class creates a Mojo connection to the Sharing interface, which causes the
// process to start up. Once the last remaining client deletes its reference,
// this class invokes the asynchronous shutdown flow, then disconnects the
// Mojo connection, which results in the process getting killed.
class NearbyProcessManagerImpl : public NearbyProcessManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyProcessManager> Create(
        NearbyDependenciesProvider* nearby_dependencies_provider,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* factory);
    virtual ~Factory() = default;

   private:
    virtual std::unique_ptr<NearbyProcessManager> BuildInstance(
        NearbyDependenciesProvider* nearby_dependencies_provider,
        std::unique_ptr<base::OneShotTimer> timer) = 0;
  };

  ~NearbyProcessManagerImpl() override;

 private:
  friend class NearbyProcessManagerImplTest;

  class NearbyReferenceImpl
      : public NearbyProcessManager::NearbyProcessReference {
   public:
    NearbyReferenceImpl(
        const mojo::SharedRemote<
            ::nearby::connections::mojom::NearbyConnections>& connections,
        const mojo::SharedRemote<
            ::ash::nearby::presence::mojom::NearbyPresence>& presence,
        const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>& decoder,
        const mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>&
            quick_start_decoder,
        base::OnceClosure destructor_callback);
    ~NearbyReferenceImpl() override;

   private:
    // NearbyProcessManager::NearbyProcessReference:
    const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
    GetNearbyConnections() const override;
    const mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>&
    GetNearbyPresence() const override;
    const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>&
    GetNearbySharingDecoder() const override;
    const mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>&
    GetQuickStartDecoder() const override;

    mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>
        connections_;
    mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>
        presence_;
    mojo::SharedRemote<sharing::mojom::NearbySharingDecoder> decoder_;
    mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>
        quick_start_decoder_;
    base::OnceClosure destructor_callback_;
  };

  NearbyProcessManagerImpl(
      NearbyDependenciesProvider* nearby_dependencies_provider,
      std::unique_ptr<base::OneShotTimer> timer,
      const base::RepeatingCallback<
          mojo::PendingRemote<sharing::mojom::Sharing>()>& sharing_binder);

  // NearbyProcessManagerImpl:
  std::unique_ptr<NearbyProcessReference> GetNearbyProcessReference(
      NearbyProcessStoppedCallback on_process_stopped_callback) override;
  void ShutDownProcess() override;

  // KeyedService:
  void Shutdown() override;

  // Returns whether the process was successfully bound.
  bool AttemptToBindToUtilityProcess();

  void OnSharingProcessCrash();
  void OnMojoPipeDisconnect(NearbyProcessShutdownReason shutdown_reason);
  void OnReferenceDeleted(const base::UnguessableToken& reference_id);
  void DoShutDownProcess(NearbyProcessShutdownReason shutdown_reason);
  void NotifyProcessStopped(NearbyProcessShutdownReason shutdown_reason);

  raw_ptr<NearbyDependenciesProvider> nearby_dependencies_provider_;
  std::unique_ptr<base::OneShotTimer> shutdown_debounce_timer_;
  base::RepeatingCallback<mojo::PendingRemote<sharing::mojom::Sharing>()>
      sharing_binder_;

  // Connection to the Nearby Connections utility process. If bound, the
  // process is active and running.
  mojo::Remote<sharing::mojom::Sharing> sharing_;

  // Implemented as SharedRemote because copies of these are intended to be used
  // by multiple clients.
  mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>
      connections_;
  mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence> presence_;
  mojo::SharedRemote<sharing::mojom::NearbySharingDecoder> decoder_;
  mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>
      quick_start_decoder_;

  // Map which stores callbacks to be invoked if the Nearby process shuts down
  // unexpectedly, before clients release their references.
  base::flat_map<base::UnguessableToken, NearbyProcessStoppedCallback>
      id_to_process_stopped_callback_map_;

  bool shut_down_ = false;

  base::WeakPtrFactory<NearbyProcessManagerImpl> weak_ptr_factory_{this};
};

}  // namespace nearby
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NEARBY_NEARBY_PROCESS_MANAGER_IMPL_H_
