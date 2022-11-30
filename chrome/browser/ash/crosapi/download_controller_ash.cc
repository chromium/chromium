// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/download_controller_ash.h"

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
  auto aggregating_downloads_callback = base::BindRepeating(
      [](mojom::DownloadControllerClient::GetAllDownloadsCallback& callback,
         size_t& num_clients_still_working,
         std::vector<mojom::DownloadItemPtr>& aggregated_downloads,
         std::vector<mojom::DownloadItemPtr> client_downloads) {
        DCHECK_GT(num_clients_still_working, 0u);

        // Aggregate downloads from each Lacros client.
        for (auto& download : client_downloads)
          aggregated_downloads.push_back(std::move(download));

        --num_clients_still_working;
        if (num_clients_still_working != 0u)
          return;

        // Sort aggregated downloads chronologically by start time.
        std::sort(aggregated_downloads.begin(), aggregated_downloads.end(),
                  [](const auto& a, const auto& b) {
                    return a->start_time.value_or(base::Time()) <
                           b->start_time.value_or(base::Time());
                  });

        std::move(callback).Run(std::move(aggregated_downloads));
      },
      base::OwnedRef(std::move(callback)),
      base::OwnedRef(size_t(clients_.size())),
      base::OwnedRef(std::vector<mojom::DownloadItemPtr>()));

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
