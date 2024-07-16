// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_BACKEND_ACCELERATOR_FETCHER_H_
#define ASH_WEBUI_COMMON_BACKEND_ACCELERATOR_FETCHER_H_

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/public/mojom/accelerator_actions.mojom.h"
#include "ash/webui/common/mojom/accelerator_fetcher.mojom.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

// Provides customized shortcut input accelerators and displays the
// accelerators in the frontend apps. Each app will create one instance
// of AcceleratorFetcherPovider.
class AcceleratorFetcher : public common::mojom::AcceleratorFetcher,
                           public AshAcceleratorConfiguration::Observer {
 public:
  AcceleratorFetcher();

  ~AcceleratorFetcher() override;

  void BindInterface(
      mojo::PendingReceiver<common::mojom::AcceleratorFetcher> receiver);
  void GetMetaKeyToDisplay(GetMetaKeyToDisplayCallback callback) override;

  // common::mojom::AcceleratorFetcher:
  void ObserveAcceleratorChanges(
      const std::vector<AcceleratorAction>& actionIds,
      mojo::PendingRemote<common::mojom::AcceleratorFetcherObserver> observer)
      override;

  // shortcut_ui::AcceleratorConfigurationProvider:
  void OnAcceleratorsUpdated() override;

  void FlushMojoForTesting();

 private:
  // Handlers for disconnection of observers.
  void OnObserverDisconnect(mojo::RemoteSetElementId id);

  mojo::Receiver<common::mojom::AcceleratorFetcher>
      accelerator_fetcher_receiver_{this};

  // Storage of accelerator actions for each remote receiver, keyed by
  // mojo::RemoteSetElementId in |accelerator_observers_|.
  std::map<mojo::RemoteSetElementId, std::vector<AcceleratorAction>>
      actions_for_receivers_;
  mojo::RemoteSet<common::mojom::AcceleratorFetcherObserver>
      accelerator_observers_;
  base::WeakPtrFactory<AcceleratorFetcher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_COMMON_BACKEND_ACCELERATOR_FETCHER_H_
