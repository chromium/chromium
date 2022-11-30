// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_CONTROLLER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_CONTROLLER_ASH_H_

#include <string>

#include "base/observer_list.h"
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
    virtual void OnLacrosDownloadCreated(const mojom::DownloadItem&) {}
    virtual void OnLacrosDownloadUpdated(const mojom::DownloadItem&) {}
    virtual void OnLacrosDownloadDestroyed(const mojom::DownloadItem&) {}
  };

  DownloadControllerAsh();
  DownloadControllerAsh(const DownloadControllerAsh&) = delete;
  DownloadControllerAsh& operator=(const DownloadControllerAsh&) = delete;
  ~DownloadControllerAsh() override;

  // Bind this receiver for `mojom::DownloadController`. This is used by
  // crosapi.
  void BindReceiver(mojo::PendingReceiver<mojom::DownloadController> receiver);

  // mojom::DownloadController:
  void BindClient(
      mojo::PendingRemote<mojom::DownloadControllerClient> client) override;
  void OnDownloadCreated(mojom::DownloadItemPtr download) override;
  void OnDownloadUpdated(mojom::DownloadItemPtr download) override;
  void OnDownloadDestroyed(mojom::DownloadItemPtr download) override;

  // Required for the below `base::ObserverList`:
  void AddObserver(DownloadControllerObserver* observer);
  void RemoveObserver(DownloadControllerObserver* observer);

  // Asynchronously returns all downloads from each Lacros client via the
  // specified `callback`, no matter the type or state. Downloads are sorted
  // chronologically by start time.
  void GetAllDownloads(
      mojom::DownloadControllerClient::GetAllDownloadsCallback callback);

  // Pauses the download associated with the specified `download_guid`. This
  // method will ultimately invoke `download::DownloadItem::Pause()`.
  void Pause(const std::string& download_guid);

  // Resumes the download associated with the specified `download_guid`. If
  // `user_resume` is set to `true`, it signifies that this invocation was
  // triggered by an explicit user action. This method will ultimately invoke
  // `download::DownloadItem::Resume()`.
  void Resume(const std::string& download_guid, bool user_resume);

  // Cancels the download associated with the specified `download_guid`.  If
  // `user_cancel` is set to `true`, it signifies that this invocation was
  // triggered by an explicit user action. This method will ultimately invoke
  // `download::DownloadItem::Cancel()`.
  void Cancel(const std::string& download_guid, bool user_cancel);

  // Marks the download associated with the specified `download_guid` to be
  // `open_when_complete`. This method will ultimately invoke
  // `download::DownloadItem::SetOpenWhenComplete()`.
  void SetOpenWhenComplete(const std::string& download_guid,
                           bool open_when_complete);

 private:
  mojo::ReceiverSet<mojom::DownloadController> receivers_;
  mojo::RemoteSet<mojom::DownloadControllerClient> clients_;
  base::ObserverList<DownloadControllerObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_CONTROLLER_ASH_H_
