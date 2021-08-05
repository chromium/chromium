// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_NEARBY_SHARE_SESSION_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_NEARBY_SHARE_SESSION_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/nearby_share/share_info_file_handler.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "components/arc/mojom/nearby_share.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/widget.h"

namespace webshare {
class PrepareDirectoryTask;
}  // namespace webshare

namespace arc {

// Implementation of NearbyShareSession interface.
class NearbyShareSessionImpl : public mojom::NearbyShareSessionHost,
                               public aura::WindowObserver,
                               public aura::EnvObserver {
 public:
  using SessionFinishedCallback = base::OnceCallback<void(int32_t)>;

  NearbyShareSessionImpl(
      Profile* profile,
      uint32_t task_id,
      mojom::ShareIntentInfoPtr share_info,
      mojo::PendingRemote<mojom::NearbyShareSessionInstance> session_instance,
      mojo::PendingReceiver<mojom::NearbyShareSessionHost> session_receiver,
      SessionFinishedCallback session_finished_callback);

  NearbyShareSessionImpl(const NearbyShareSessionImpl&) = delete;
  NearbyShareSessionImpl& operator=(const NearbyShareSessionImpl&) = delete;
  ~NearbyShareSessionImpl() override;

  // Called when Nearby Share is closed.
  void OnNearbyShareClosed(views::Widget::ClosedReason reason);

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* const window) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* const window,
                                 bool visible) override;

 private:
  // Called once an ARC window is found for the given |task_id_|. This will
  // either prepare files or directly show the Nearby Share bubble.
  void OnArcWindowFound(aura::Window* const arc_window);

  // Converts |share_info_| to |apps::mojom::IntentPtr| type.
  apps::mojom::IntentPtr ConvertShareIntentInfoToIntent() const;

  void OnNearbyShareBubbleShown(sharesheet::SharesheetResult result);

  // Called when top level directory for Nearby Share cache files is created.
  void OnPreparedDirectory(aura::Window* const arc_window,
                           base::File::Error result);

  // Calls |SharesheetService.ShowNearbyShareBubble()| to start the Chrome
  // Nearby Share user flow and display bubble in ARC window.
  void ShowNearbyShareBubbleInArcWindow(
      aura::Window* const arc_window,
      absl::optional<base::File::Error> result = absl::nullopt);

  // Called back once the session duration exceeds the maximum duration.
  void OnTimerFired();

  // Called when progress bar UI update is available.
  void OnProgressBarUpdate(double value);

  // Called when the |session_receiver_| is disconnected, and closes the
  // Nearby Share bubble.
  void OnSessionDisconnected();

  // Called when shared files are no longer used by Nearby Share and can be
  // cleaned up along with the share session.
  void OnCleanupSession();

  // Android activity's task ID
  uint32_t task_id_;

  // Used to send messages to ARC.
  mojo::Remote<mojom::NearbyShareSessionInstance> session_instance_;

  // Used to bind the NearbyShareSessionHost interface implementation to a
  // message pipe.
  mojo::Receiver<mojom::NearbyShareSessionHost> session_receiver_;

  // Contents to be shared.
  mojom::ShareIntentInfoPtr share_info_;

  // Unowned pointer.
  Profile* profile_;

  // Created and lives on the UI thread but is destructed on the IO thread.
  scoped_refptr<ShareInfoFileHandler> file_handler_;

  std::unique_ptr<webshare::PrepareDirectoryTask> prepare_directory_task_;

  // Timer used to wait for the ARC window to be asynchronously initialized and
  // visible.
  base::OneShotTimer window_initialization_timer_;

  // Sequenced task runner for executing backend file IO cleanup tasks.
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Observes the ARC window.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      arc_window_observation_{this};

  // Observes the Aura environment.
  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};

  // Callback when the Nearby Share Session is finished and no longer needed.
  SessionFinishedCallback session_finished_callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<NearbyShareSessionImpl> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_NEARBY_SHARE_SESSION_IMPL_H_
