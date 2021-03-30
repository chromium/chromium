// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_DOWNLOAD_MANAGER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_DOWNLOAD_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/download/public/background_service/download_params.h"

namespace download {
class DownloadService;
}  // namespace download

namespace optimization_guide {

class PredictionModelDownloadClient;
class PredictionModelDownloadObserver;

namespace proto {
class PredictionModel;
}  // namespace proto

// Manages the downloads of prediction models.
class PredictionModelDownloadManager {
 public:
  PredictionModelDownloadManager(
      download::DownloadService* download_service,
      const base::FilePath& models_dir,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
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

  // Adds and removes observers.
  //
  // All methods called on observers will be invoked on the UI thread.
  virtual void AddObserver(PredictionModelDownloadObserver* observer);
  virtual void RemoveObserver(PredictionModelDownloadObserver* observer);

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

  // Verifies the download came from a trusted source and process the downloaded
  // contents. Returns a pair of file paths of the form (src, dst) if
  // |file_path| is successfully verified.
  //
  // Must be called on the background thread, as it performs file I/O.
  base::Optional<std::pair<base::FilePath, base::FilePath>> ProcessDownload(
      const base::FilePath& file_path);

  // Starts unzipping the contents of |unzip_paths|, if present. |unzip_paths|
  // is a pair of the form (src, dst), if present.
  void StartUnzipping(
      const base::Optional<std::pair<base::FilePath, base::FilePath>>&
          unzip_paths);

  // Invoked when the contents of |original_file_path| have been unzipped to
  // |unzipped_dir_path|.
  void OnDownloadUnzipped(const base::FilePath& original_file_path,
                          const base::FilePath& unzipped_dir_path,
                          bool success);

  // Processes the contents in |unzipped_dir_path|.
  //
  // Must be called on the background thread, as it performs file I/O.
  base::Optional<proto::PredictionModel> ProcessUnzippedContents(
      const base::FilePath& unzipped_dir_path);

  // Notifies |observers_| that a model is ready.
  //
  // Must be invoked on the UI thread.
  void NotifyModelReady(const base::Optional<proto::PredictionModel>& model);

  // The set of GUIDs that are still pending download.
  std::set<std::string> pending_download_guids_;

  // The Download Service to schedule model downloads with.
  //
  // Guaranteed to outlive |this|.
  download::DownloadService* download_service_;

  // The directory to store verified models in.
  base::FilePath models_dir_;

  // Whether the download service is available.
  bool is_available_for_downloads_;

  // The API key to attach to download requests.
  std::string api_key_;

  // The set of observers to be notified of completed downloads.
  base::ObserverList<PredictionModelDownloadObserver> observers_;

  // Whether the download should be verified. Should only be false for testing.
  bool should_verify_download_ = true;

  // Background thread where download file processing should be performed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Sequence checker used to verify all public API methods are called on the
  // UI thread.
  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get weak ptr to self on the UI thread.
  base::WeakPtrFactory<PredictionModelDownloadManager> ui_weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_DOWNLOAD_MANAGER_H_
