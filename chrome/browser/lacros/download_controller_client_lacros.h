// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DOWNLOAD_CONTROLLER_CLIENT_LACROS_H_
#define CHROME_BROWSER_LACROS_DOWNLOAD_CONTROLLER_CLIENT_LACROS_H_

#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/download/notification/multi_profile_download_notifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class DownloadManager;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

// This class receives and forwards download events to Ash. It can only be
// used on the main thread. In the near future, it will also be the receiver for
// calls to pause, cancel, and resume downloads from ash-chrome, hence the name.
class DownloadControllerClientLacros
    : public crosapi::mojom::DownloadControllerClient,
      public MultiProfileDownloadNotifier::Client,
      public ProfileManagerObserver {
 public:
  DownloadControllerClientLacros();
  DownloadControllerClientLacros(const DownloadControllerClientLacros&) =
      delete;
  DownloadControllerClientLacros& operator=(
      const DownloadControllerClientLacros&) = delete;
  ~DownloadControllerClientLacros() override;

 private:
  // crosapi::mojom::DownloadControllerClient:
  void GetAllDownloads(
      crosapi::mojom::DownloadControllerClient::GetAllDownloadsCallback
          callback) override;
  void Pause(const std::string& download_guid) override;
  void Resume(const std::string& download_guid, bool user_resume) override;
  void Cancel(const std::string& download_guid, bool user_cancel) override;
  void SetOpenWhenComplete(const std::string& download_guid,
                           bool open_when_complete) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // MultiProfileDownloadNotifier::Client:
  void OnManagerInitialized(content::DownloadManager* manager) override;
  void OnManagerGoingDown(content::DownloadManager* manager) override;
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadDestroyed(content::DownloadManager* manager,
                           download::DownloadItem* item) override;

  mojo::Receiver<crosapi::mojom::DownloadControllerClient> client_receiver_{
      this};
  MultiProfileDownloadNotifier download_notifier_{
      this, /*wait_for_manager_initialization=*/true};
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

#endif  // CHROME_BROWSER_LACROS_DOWNLOAD_CONTROLLER_CLIENT_LACROS_H_
