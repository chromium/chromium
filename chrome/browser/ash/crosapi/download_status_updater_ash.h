// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_STATUS_UPDATER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_STATUS_UPDATER_ASH_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace ash::download_status {
class DisplayManager;
}  // namespace ash::download_status

namespace crosapi {

// The implementation of the interface which allows Lacros download status
// updates to be passed into Ash Chrome for rendering in the appropriate System
// UI surface(s).
class DownloadStatusUpdaterAsh : public mojom::DownloadStatusUpdater {
 public:
  explicit DownloadStatusUpdaterAsh(Profile* profile);
  DownloadStatusUpdaterAsh(const DownloadStatusUpdaterAsh&) = delete;
  DownloadStatusUpdaterAsh& operator=(const DownloadStatusUpdaterAsh&) = delete;
  ~DownloadStatusUpdaterAsh() override;

  // Binds the specified pending receiver to `this` for use by crosapi.
  void BindReceiver(mojo::PendingReceiver<mojom::DownloadStatusUpdater>);

  // Attempts to cancel/pause/resume/show the download associated with the
  // specified `guid` in the browser, invoking `callback` to return whether the
  // command was handled successfully. Note that `callback` may be invoked
  // asynchronously.
  void Cancel(const std::string& guid,
              mojom::DownloadStatusUpdaterClient::CancelCallback callback);
  void Pause(const std::string& guid,
             mojom::DownloadStatusUpdaterClient::PauseCallback callback);
  void Resume(const std::string& guid,
              mojom::DownloadStatusUpdaterClient::ResumeCallback callback);
  void ShowInBrowser(
      const std::string& guid,
      mojom::DownloadStatusUpdaterClient::ShowInBrowserCallback callback);

 private:
  // mojom::DownloadStatusUpdater:
  void BindClient(
      mojo::PendingRemote<mojom::DownloadStatusUpdaterClient> client) override;
  void Update(mojom::DownloadStatusPtr status) override;

  // Callback which accepts whether a function call was `handled` successfully.
  using HandledCallback = base::OnceCallback<void(bool handled)>;

  // Pointer to a `mojom::DownloadStatusUpdaterClient` function which accepts a
  // `guid` for a download and a `callback` accepting whether the function call
  // was handled successfully. Note that `callback` may be invoked
  // asynchronously.
  using DownloadStatusUpdaterClientFunction =
      void (mojom::DownloadStatusUpdaterClient::*)(const std::string& guid,
                                                   HandledCallback callback);

  // Invokes the specified `mojom::DownloadStatusUpdaterClient` function with
  // the specified `guid`, invoking `callback` to return whether the function
  // call was handled successfully. Note that `callback` may be invoked
  // asynchronously.
  void Invoke(DownloadStatusUpdaterClientFunction func,
              const std::string& guid,
              HandledCallback callback);

  // Displays download updates in Ash. Created only when the downloads
  // integration V2 feature is enabled.
  std::unique_ptr<ash::download_status::DisplayManager> display_manager_;

  // The set of receivers bound to `this` for use by crosapi.
  mojo::ReceiverSet<mojom::DownloadStatusUpdater> receivers_;

  // The set of remotely bound clients for use by crosapi.
  mojo::RemoteSet<mojom::DownloadStatusUpdaterClient> clients_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DOWNLOAD_STATUS_UPDATER_ASH_H_
