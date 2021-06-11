// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_NEARBY_SHARE_SESSION_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_NEARBY_SHARE_SESSION_IMPL_H_

#include <cstdint>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "components/arc/mojom/nearby_share.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace arc {

// Implementation of NearbyShareSession interface.
class NearbyShareSessionImpl : public mojom::NearbyShareSessionHost,
                               public aura::WindowObserver,
                               public aura::EnvObserver {
 public:
  static mojo::PendingRemote<mojom::NearbyShareSessionHost> Create(
      Profile* profile,
      int32_t task_id,
      mojom::ShareIntentInfoPtr share_info,
      mojo::PendingRemote<mojom::NearbyShareSessionInstance> instance);

  NearbyShareSessionImpl(const NearbyShareSessionImpl&) = delete;
  NearbyShareSessionImpl& operator=(const NearbyShareSessionImpl&) = delete;
  ~NearbyShareSessionImpl() override;

  // Called when NearbyShare is closed.
  void OnNearbyShareClosed();

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

 private:
  NearbyShareSessionImpl(
      Profile* profile,
      int32_t task_id,
      mojom::ShareIntentInfoPtr share_info,
      mojo::PendingRemote<mojom::NearbyShareSessionInstance> session_instance,
      mojo::PendingReceiver<mojom::NearbyShareSessionHost> receiver);

  // Calls |SharesheetService.ShowNearbyShareBubble()| to start the Chrome
  // Nearby Share user flow.
  void ShowNearbyBubble(aura::Window* arc_window);

  // Converts |share_info_| to |apps::mojom::IntentPtr| type.
  apps::mojom::IntentPtr ConvertShareIntentInfoToIntentFilter() const;

  void OnNearbyShareBubbleShown(sharesheet::SharesheetResult result);

  // Called back once the session duration exceeds the maximum duration.
  void OnTimerFired();

  // Android activity's task ID
  int32_t task_id_;

  // Used to send messages to ARC.
  mojo::Remote<mojom::NearbyShareSessionInstance> session_instance_;

  // Used to bind the NearbyShareSessionHost interface implementation to a
  // message pipe.
  mojo::Receiver<mojom::NearbyShareSessionHost> session_receiver_;

  // Contents to be shared.
  mojom::ShareIntentInfoPtr share_info_;

  // Unowned pointer.
  Profile* profile_;

  // Timer used to wait for the ARC window to be asynchronously initialized and
  // visible.
  base::OneShotTimer window_initialization_timer_;

  // Observes the ARC window.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      arc_window_observation_{this};

  // Observes the Aura environment.
  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<NearbyShareSessionImpl> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_NEARBY_SHARE_SESSION_IMPL_H_
