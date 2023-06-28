// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/download_status_updater_ash.h"

#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/callback.h"
#include "base/functional/identity.h"
#include "base/functional/invoke.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace crosapi {

DownloadStatusUpdaterAsh::DownloadStatusUpdaterAsh() = default;

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

// TODO(http://b/279831939): Render in the appropriate System UI surface(s).
void DownloadStatusUpdaterAsh::Update(mojom::DownloadStatusPtr status) {
  NOTIMPLEMENTED();
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
            return base::ranges::any_of(handled_by_client, base::identity());
          }).Then(std::move(callback)));

  for (auto& client : clients_) {
    base::invoke(func, client.get(), guid,
                 mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                     base::BindOnce(handled_by_client_callback),
                     /*handled_by_client=*/false));
  }
}

}  // namespace crosapi
