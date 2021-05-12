// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_CONTROLLER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_CONTROLLER_ASH_H_

#include "base/observer_list_types.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// The ash-chrome implementation of the DownloadController crosapi interface.
// This is where ash-chrome receives information on download events from lacros.
// This class must only be used from the main thread.
class DownloadControllerAsh : public mojom::DownloadController {
 public:
  // Allows ash classes to observe download events.
  class DownloadControllerObserver : public base::CheckedObserver {
   public:
    virtual void OnLacrosDownloadCreated(const mojom::DownloadEvent& event) {}
    virtual void OnLacrosDownloadUpdated(const mojom::DownloadEvent& event) {}
    virtual void OnLacrosDownloadDestroyed(const mojom::DownloadEvent& event) {}
  };

  DownloadControllerAsh();
  DownloadControllerAsh(const DownloadControllerAsh&) = delete;
  DownloadControllerAsh& operator=(const DownloadControllerAsh&) = delete;
  ~DownloadControllerAsh() override;

  // Bind this receiver for `mojom::DownloadController`. This is used by
  // crosapi.
  void BindReceiver(mojo::PendingReceiver<mojom::DownloadController> receiver);

  // mojom::DownloadController:
  void OnDownloadCreated(mojom::DownloadEventPtr event) override;
  void OnDownloadUpdated(mojom::DownloadEventPtr event) override;
  void OnDownloadDestroyed(mojom::DownloadEventPtr event) override;

  // Required for the below `base::ObserverList`:
  void AddObserver(DownloadControllerObserver* observer);
  void RemoveObserver(DownloadControllerObserver* observer);

 private:
  mojo::ReceiverSet<mojom::DownloadController> receivers_;
  base::ObserverList<DownloadControllerObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_CONTROLLER_ASH_H_
