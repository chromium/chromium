// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_IME_SERVICE_CONNECTOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_IME_SERVICE_CONNECTOR_H_

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/ash/services/ime/public/mojom/ime_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {
namespace input_method {

// The connector of an ImeService which runs in its own process.
class ImeServiceConnector : public ime::mojom::PlatformAccessProvider,
                            public ProfileObserver {
 public:
  explicit ImeServiceConnector(Profile* profile);

  ImeServiceConnector(const ImeServiceConnector&) = delete;
  ImeServiceConnector& operator=(const ImeServiceConnector&) = delete;

  ~ImeServiceConnector() override;

  // ash::ime::mojom::PlatformAccessProvider overrides:
  void DownloadImeFileTo(const GURL& url,
                         const base::FilePath& file_path,
                         DownloadImeFileToCallback callback) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Launch an out-of-process IME service and grant necessary Platform access.
  void SetupImeService(
      mojo::PendingReceiver<ime::mojom::InputEngineManager> receiver);

  void BindInputMethodUserDataService(
      mojo::PendingReceiver<ime::mojom::InputMethodUserDataService> receiver);

  void OnFileDownloadComplete(DownloadImeFileToCallback client_callback,
                              base::FilePath path);

 private:
  void MaybeTriggerDownload(GURL url,
                            base::FilePath file_path,
                            DownloadImeFileToCallback callback);

  void HandleDownloadResponse(base::FilePath file_path);
  void NotifyAllDownloadListeners(base::FilePath file_path);

  raw_ptr<Profile> profile_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The current request in progress, or NULL.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // The url of the current in progress request.
  std::optional<std::string> active_request_url_;

  // Callbacks collected for simultaneous requests for the same download url.
  std::vector<DownloadImeFileToCallback> download_callbacks_;

  // Persistent connection to the IME service process.
  mojo::Remote<ime::mojom::ImeService> remote_service_;
  mojo::Receiver<ime::mojom::PlatformAccessProvider> platform_access_receiver_{
      this};

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<ImeServiceConnector> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_IME_SERVICE_CONNECTOR_H_
