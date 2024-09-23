// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/download_controller_ash.h"

#include "base/barrier_callback.h"
#include "base/containers/extend.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace crosapi {

DownloadControllerAsh::DownloadControllerAsh() = default;
DownloadControllerAsh::~DownloadControllerAsh() = default;

void DownloadControllerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DownloadController> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DownloadControllerAsh::BindClient(
    mojo::PendingRemote<mojom::DownloadControllerClient> client) {
  clients_.Add(std::move(client));
}

void DownloadControllerAsh::OnDownloadCreated(
    crosapi::mojom::DownloadItemPtr download) {
  for (auto& observer : observers_)
    observer.OnLacrosDownloadCreated(*download);
}

void DownloadControllerAsh::OnDownloadUpdated(
    crosapi::mojom::DownloadItemPtr download) {
  for (auto& observer : observers_)
    observer.OnLacrosDownloadUpdated(*download);
}

void DownloadControllerAsh::OnDownloadDestroyed(
    crosapi::mojom::DownloadItemPtr download) {
  for (auto& observer : observers_)
    observer.OnLacrosDownloadDestroyed(*download);
}

void DownloadControllerAsh::AddObserver(DownloadControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void DownloadControllerAsh::RemoveObserver(
    DownloadControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DownloadControllerAsh::GetAllDownloads(
    mojom::DownloadControllerClient::GetAllDownloadsCallback callback) {
  if (clients_.empty()) {
    std::move(callback).Run({});
    return;
  }

  // This callback will be invoked by each Lacros client to aggregate all
  // downloads, sort them chronologically by start time, and ultimately provide
  // them to the original `callback`.
  auto aggregating_downloads_callback =
      base::BarrierCallback<std::vector<mojom::DownloadItemPtr>>(
          clients_.size(),
          base::BindOnce([](std::vector<std::vector<mojom::DownloadItemPtr>>
                                client_downloads) {
            std::vector<mojom::DownloadItemPtr> aggregated_downloads;

            // Aggregate downloads from each Lacros client.
            for (auto& entry : client_downloads) {
              base::Extend(aggregated_downloads, std::move(entry));
            }

            // Sort aggregated downloads chronologically by start time.
            // `start_time` equal to `std::nullopt` is by default less than any
            // non-empty `start_time`.
            base::ranges::sort(aggregated_downloads, base::ranges::less{},
                               &mojom::DownloadItem::start_time);

            return aggregated_downloads;
          }).Then(std::move(callback)));

  // Aggregate downloads from each Lacros `client`. Note that if the `client` is
  // not of a supported `version` or if the connection is dropped before the
  // `client` returns a response, it will not contribute any downloads.
  for (auto& client : clients_) {
    const uint32_t version = client.version();
    if (mojom::DownloadControllerClient::kGetAllDownloadsMinVersion > version) {
      aggregating_downloads_callback.Run(std::vector<mojom::DownloadItemPtr>());
      continue;
    }
    client->GetAllDownloads(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
        base::BindOnce(aggregating_downloads_callback),
        /*default_invoke_args=*/std::vector<mojom::DownloadItemPtr>()));
  }
}

void DownloadControllerAsh::Pause(const std::string& download_guid) {
  for (auto& client : clients_)
    client->Pause(download_guid);
}

void DownloadControllerAsh::Resume(const std::string& download_guid,
                                   bool user_resume) {
  for (auto& client : clients_)
    client->Resume(download_guid, user_resume);
}

void DownloadControllerAsh::Cancel(const std::string& download_guid,
                                   bool user_cancel) {
  for (auto& client : clients_)
    client->Cancel(download_guid, user_cancel);
}

void DownloadControllerAsh::SetOpenWhenComplete(
    const std::string& download_guid,
    bool open_when_complete) {
  for (auto& client : clients_)
    client->SetOpenWhenComplete(download_guid, open_when_complete);
}

}  // namespace crosapi
