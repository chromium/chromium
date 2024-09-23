// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/download_status_updater_ash.h"

#include <functional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/barrier_callback.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/ash/download_status/display_manager.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace crosapi {

DownloadStatusUpdaterAsh::DownloadStatusUpdaterAsh(Profile* profile) {
  display_manager_ =
      std::make_unique<ash::download_status::DisplayManager>(profile, this);
}

DownloadStatusUpdaterAsh::~DownloadStatusUpdaterAsh() = default;

void DownloadStatusUpdaterAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DownloadStatusUpdater> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DownloadStatusUpdaterAsh::Cancel(
    const std::string& guid,
    mojom::DownloadStatusUpdaterClient::CancelCallback callback) {
  Invoke(&mojom::DownloadStatusUpdaterClient::Cancel, guid,
         std::move(callback));
}

void DownloadStatusUpdaterAsh::Pause(
    const std::string& guid,
    mojom::DownloadStatusUpdaterClient::PauseCallback callback) {
  Invoke(&mojom::DownloadStatusUpdaterClient::Pause, guid, std::move(callback));
}

void DownloadStatusUpdaterAsh::Resume(
    const std::string& guid,
    mojom::DownloadStatusUpdaterClient::ResumeCallback callback) {
  Invoke(&mojom::DownloadStatusUpdaterClient::Resume, guid,
         std::move(callback));
}

void DownloadStatusUpdaterAsh::ShowInBrowser(
    const std::string& guid,
    mojom::DownloadStatusUpdaterClient::ShowInBrowserCallback callback) {
  Invoke(&mojom::DownloadStatusUpdaterClient::ShowInBrowser, guid,
         std::move(callback));
}

void DownloadStatusUpdaterAsh::BindClient(
    mojo::PendingRemote<mojom::DownloadStatusUpdaterClient> client) {
  clients_.Add(std::move(client));
}

void DownloadStatusUpdaterAsh::Update(mojom::DownloadStatusPtr status) {
  if (display_manager_) {
    display_manager_->Update(*status);
  }
}

void DownloadStatusUpdaterAsh::Invoke(DownloadStatusUpdaterClientFunction func,
                                      const std::string& guid,
                                      HandledCallback callback) {
  if (clients_.empty()) {
    std::move(callback).Run(/*handled=*/false);
    return;
  }

  // This callback is invoked by each client to indicate whether it handled the
  // `func` call successfully. Once all clients have responded, the `callback`
  // supplied to `this` method is run to indicate whether *any* client handled
  // the `func` call successfully.
  base::RepeatingCallback<void(bool)> handled_by_client_callback =
      base::BarrierCallback<bool>(
          clients_.size(),
          base::BindOnce([](std::vector<bool> handled_by_client) {
            return base::ranges::any_of(handled_by_client, std::identity());
          }).Then(std::move(callback)));

  for (auto& client : clients_) {
    std::invoke(func, client.get(), guid,
                mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                    base::BindOnce(handled_by_client_callback),
                    /*handled_by_client=*/false));
  }
}

}  // namespace crosapi
