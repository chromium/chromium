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

void DownloadControllerAsh::OnDownloadCreated(
    crosapi::mojom::DownloadEventPtr event) {
  for (auto& observer : observers_)
    observer.OnLacrosDownloadCreated(*event);
}

void DownloadControllerAsh::OnDownloadUpdated(
    crosapi::mojom::DownloadEventPtr event) {
  for (auto& observer : observers_)
    observer.OnLacrosDownloadUpdated(*event);
}

void DownloadControllerAsh::OnDownloadDestroyed(
    crosapi::mojom::DownloadEventPtr event) {
  for (auto& observer : observers_)
    observer.OnLacrosDownloadDestroyed(*event);
}

void DownloadControllerAsh::AddObserver(DownloadControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void DownloadControllerAsh::RemoveObserver(
    DownloadControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace crosapi
