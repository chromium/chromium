// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/screen_ai_downloader_ash.h"

#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

ScreenAIDownloaderAsh::ScreenAIDownloaderAsh() {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &ScreenAIDownloaderAsh::OnDisconnected, weak_factory_.GetWeakPtr()));
}

ScreenAIDownloaderAsh::~ScreenAIDownloaderAsh() = default;

void ScreenAIDownloaderAsh::Bind(
    mojo::PendingReceiver<crosapi::mojom::ScreenAIDownloader>
        screen_ai_downloader) {
  receivers_.Add(this, std::move(screen_ai_downloader));
}

void ScreenAIDownloaderAsh::GetComponentFolder(
    bool download_if_needed,
    GetComponentFolderCallback callback) {
  auto* install_state = screen_ai::ScreenAIInstallState::GetInstance();

  if (install_state->IsComponentAvailable()) {
    std::move(callback).Run(
        install_state->get_component_binary_path().DirName());
    return;
  }

  if (!download_if_needed) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Keep the callback and observe status updates.
  mojo::ReceiverId id = receivers_.current_receiver();
  pending_download_callback_map_.emplace(id, std::move(callback));
  if (!install_state_observer_.IsObserving()) {
    // Adding the observer will trigger download.
    install_state_observer_.Observe(install_state);
  } else {
    // When the observer is added and the component does not exit, it means that
    // download has failed the previous time. So we need to try again.
    install_state->DownloadComponent();
  }
}

void ScreenAIDownloaderAsh::DownloadComponentDeprecated(
    DownloadComponentDeprecatedCallback callback) {
  GetComponentFolder(/*download_if_needed=*/true, std::move(callback));
}

void ScreenAIDownloaderAsh::SetLastUsageTime() {
  screen_ai::ScreenAIInstallState::GetInstance()->SetLastUsageTime();
}

void ScreenAIDownloaderAsh::StateChanged(
    screen_ai::ScreenAIInstallState::State state) {
  if (pending_download_callback_map_.empty()) {
    return;
  }

  std::optional<base::FilePath> component_path = std::nullopt;

  switch (state) {
    case screen_ai::ScreenAIInstallState::State::kNotDownloaded:
    case screen_ai::ScreenAIInstallState::State::kDownloading:
      return;

    case screen_ai::ScreenAIInstallState::State::kDownloadFailed:
      break;

    case screen_ai::ScreenAIInstallState::State::kDownloaded:
      component_path = screen_ai::ScreenAIInstallState::GetInstance()
                           ->get_component_binary_path()
                           .DirName();
      break;
  }

  for (auto& entry : pending_download_callback_map_) {
    std::move(entry.second).Run(component_path);
  }
  pending_download_callback_map_.clear();
}

void ScreenAIDownloaderAsh::OnDisconnected() {
  mojo::ReceiverId id = receivers_.current_receiver();
  pending_download_callback_map_.erase(id);
}

}  // namespace crosapi
