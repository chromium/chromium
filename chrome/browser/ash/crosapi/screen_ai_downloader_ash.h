// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SCREEN_AI_DOWNLOADER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SCREEN_AI_DOWNLOADER_ASH_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chromeos/crosapi/mojom/screen_ai_downloader.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi interface for ScreenAI downloader. Lives in Ash-Chrome
// on the UI thread.
class ScreenAIDownloaderAsh : public mojom::ScreenAIDownloader,
                              public screen_ai::ScreenAIInstallState::Observer {
 public:
  ScreenAIDownloaderAsh();
  ScreenAIDownloaderAsh(const ScreenAIDownloaderAsh&) = delete;
  ScreenAIDownloaderAsh& operator=(const ScreenAIDownloaderAsh&) = delete;
  ~ScreenAIDownloaderAsh() override;

  void Bind(mojo::PendingReceiver<crosapi::mojom::ScreenAIDownloader>
                screen_ai_downloader);

 private:
  friend class ScreenAIDownloaderAshTest;
  friend class ScreenAIDownloaderAshReplyTest;

  // crosapi::mojom::ScreenAIDownloader:
  void DownloadComponentDeprecated(
      DownloadComponentDeprecatedCallback callback) override;
  void GetComponentFolder(bool download_if_needed,
                          GetComponentFolderCallback callback) override;
  void SetLastUsageTime() override;

  // screen_ai::ScreenAIInstallState::Observer:
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override;

  // Called when a connection is lost.
  void OnDisconnected();

  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      install_state_observer_{this};

  // This map keeps a callback function for a receiver. In `OnDisconnected()`,
  // this map is used to clear out a pending callback related to the destroyed
  // receiver.
  std::map<mojo::ReceiverId, GetComponentFolderCallback>
      pending_download_callback_map_;

  // This set maintains a list of all receivers.
  mojo::ReceiverSet<mojom::ScreenAIDownloader> receivers_;

  base::WeakPtrFactory<ScreenAIDownloaderAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_SCREEN_AI_DOWNLOADER_ASH_H_
