// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/download_controller_ash.h"

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
