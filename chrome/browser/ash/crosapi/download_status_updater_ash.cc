// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/download_status_updater_ash.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace crosapi {

DownloadStatusUpdaterAsh::DownloadStatusUpdaterAsh() = default;

DownloadStatusUpdaterAsh::~DownloadStatusUpdaterAsh() = default;

void DownloadStatusUpdaterAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DownloadStatusUpdater> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DownloadStatusUpdaterAsh::BindClient(
    mojo::PendingRemote<mojom::DownloadStatusUpdaterClient> client) {
  clients_.Add(std::move(client));
}

// TODO(http://b/279831939): Render in the appropriate System UI surface(s).
void DownloadStatusUpdaterAsh::Update(mojom::DownloadStatusPtr status) {
  NOTIMPLEMENTED();
}

}  // namespace crosapi
