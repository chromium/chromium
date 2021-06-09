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
#include "chrome/browser/ui/ash/arc_custom_tab_modal_dialog_host.h"
#include "components/arc/mojom/nearby_share.mojom-forward.h"
#include "components/arc/mojom/nearby_share.mojom.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace arc {

// Implementation of NearbyShareSession interface.
class NearbyShareSessionImpl
    : public mojom::NearbyShareSessionHost,
      public content::WebContentsUserData<NearbyShareSessionImpl>,
      public ArcCustomTabModalDialogHost,
      public aura::WindowObserver,
      public aura::EnvObserver {
 public:
  static mojo::PendingRemote<mojom::NearbyShareSessionHost> Create(
      std::unique_ptr<content::WebContents> web_contents,
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

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  friend class content::WebContentsUserData<NearbyShareSessionImpl>;

  NearbyShareSessionImpl(
      content::WebContents* web_contents_ptr,
      std::unique_ptr<content::WebContents> web_contents,
      int32_t task_id,
      mojom::ShareIntentInfoPtr share_info,
      mojo::PendingRemote<mojom::NearbyShareSessionInstance> session_instance,
      mojo::PendingReceiver<mojom::NearbyShareSessionHost> receiver);

  // Creates the Chrome Custom Tab and calls
  // |SharesheetService.ShowNearbyShareBubble()| to start the Chrome Nearby
  // Share user flow.
  void ShowNearbyBubble(aura::Window* arc_window);

  // Converts |share_info_| to |apps::mojom::IntentPtr| type.
  apps::mojom::IntentPtr ConvertShareIntentInfoToIntentFilter() const;

  void OnNearbyShareBubbleShown(sharesheet::SharesheetResult result);

  // Called back once the session duration exceeds the maximum duration.
  void OnTimerFired();

  // Close the ARC Custom Tab and NearbyShare Bubble if remote connection
  // closes.
  void Close();

  // Android activity's task ID
  int32_t task_id_;

  // Used to send messages to ARC.
  mojo::Remote<mojom::NearbyShareSessionInstance> session_instance_;

  // Used to bind the NearbyShareSessionHost interface implementation to a
  // message pipe.
  mojo::Receiver<mojom::NearbyShareSessionHost> session_receiver_;

  // Contents to be shared.
  mojom::ShareIntentInfoPtr share_info_;

  // Web contents for the ARC custom tab.
  std::unique_ptr<content::WebContents> web_contents_;

  // Timer used to wait for the ARC window to be asynchronously initialized and
  // visible.
  base::OneShotTimer window_initialization_timer_;

  // Observes the ARC window.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      arc_window_observation_{this};

  // Observes the Aura environment.
  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<NearbyShareSessionImpl> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_NEARBY_SHARE_SESSION_IMPL_H_
