// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_STATUS_UPDATER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_STATUS_UPDATER_ASH_H_

#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The implementation of the interface which allows Lacros download status
// updates to be passed into Ash Chrome for rendering in the appropriate System
// UI surface(s).
class DownloadStatusUpdaterAsh : public mojom::DownloadStatusUpdater {
 public:
  DownloadStatusUpdaterAsh();
  DownloadStatusUpdaterAsh(const DownloadStatusUpdaterAsh&) = delete;
  DownloadStatusUpdaterAsh& operator=(const DownloadStatusUpdaterAsh&) = delete;
  ~DownloadStatusUpdaterAsh() override;

  // Binds the specified pending receiver to `this` for use by crosapi.
  void BindReceiver(mojo::PendingReceiver<mojom::DownloadStatusUpdater>);

 private:
  // DownloadStatusUpdater:
  void Update(mojom::DownloadStatusPtr status) override;

  // The set of receivers bound to `this` for use by crosapi.
  mojo::ReceiverSet<mojom::DownloadStatusUpdater> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_STATUS_UPDATER_ASH_H_
