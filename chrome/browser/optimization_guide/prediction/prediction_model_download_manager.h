// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_DOWNLOAD_MANAGER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_DOWNLOAD_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/download/public/background_service/download_params.h"

class Profile;

namespace download {
class DownloadService;
}  // namespace download

namespace optimization_guide {

class PredictionModelDownloadClient;

// Manages the downloads of prediction models.
class PredictionModelDownloadManager {
 public:
  explicit PredictionModelDownloadManager(Profile* profile);
  virtual ~PredictionModelDownloadManager();
  PredictionModelDownloadManager(const PredictionModelDownloadManager&) =
      delete;
  PredictionModelDownloadManager& operator=(
      const PredictionModelDownloadManager&) = delete;

  // Starts a download for |download_url|.
  virtual void StartDownload(const GURL& download_url);

  // Cancels all pending downloads.
  virtual void CancelAllPendingDownloads();

  // Returns whether the downloader can download models.
  virtual bool IsAvailableForDownloads() const;

 private:
  friend class PredictionModelDownloadClient;
  friend class PredictionModelDownloadManagerTest;

  // Invoked when the Download Service is ready.
  //
  // |pending_download_guids| is the set of GUIDs that were previously scheduled
  // to be downloaded and have still not been downloaded yet.
  // |successful_downloads| is the map from GUID to the file path that it was
  // successfully downloaded to.
  void OnDownloadServiceReady(
      const std::set<std::string>& pending_download_guids,
      const std::map<std::string, base::FilePath>& successful_downloads);

  // Invoked when the Download Service fails to initialize and should not be
  // used for the session.
  void OnDownloadServiceUnavailable();

  // Invoked when the download has been accepted and persisted by the
  // DownloadService.
  void OnDownloadStarted(const std::string& guid,
                         download::DownloadParams::StartResult start_result);

  // Invoked when the download as specified by |downloaded_guid| succeeded.
  void OnDownloadSucceeded(const std::string& downloaded_guid,
                           const base::FilePath& file_path);

  // Invoked when the download as specified by |failed_download_guid| failed.
  void OnDownloadFailed(const std::string& failed_download_guid);

  // The set of GUIDs that are still pending download.
  std::set<std::string> pending_download_guids_;

  // The Download Service to schedule model downloads with.
  //
  // Guaranteed to outlive |this|.
  download::DownloadService* download_service_;

  // Whether the download service is available.
  bool is_available_for_downloads_;

  // The API key to attach to download requests.
  std::string api_key_;

  base::WeakPtrFactory<PredictionModelDownloadManager> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_DOWNLOAD_MANAGER_H_
