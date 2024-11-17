// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MANAGED_SCREENSAVER_IMAGE_DOWNLOADER_H_
#define ASH_AMBIENT_MANAGED_SCREENSAVER_IMAGE_DOWNLOADER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace ash {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ScreensaverImageDownloadResult {
  kSuccess = 0,
  kNetworkError = 1,
  kFileSaveError = 2,
  kFileSystemWriteError = 3,
  kCancelled = 4,
  kMaxValue = kCancelled,
};

// Provides a cache service to download and store external image files that will
// be displayed in the managed screensaver feature. This cache will operate in a
// specific file directory, specified on instantiation.
//
// Each image will be downloaded and stored with a unique name based on its URL
// address. This cache assumes that the remote contents of the URL will not
// change, i.e. once downloaded, it will not attempt to refresh its content.
class ASH_EXPORT ScreensaverImageDownloader {
 private:
  // Expresses the state of the downloading queue. It has two states:
  //   * Waiting: No download is being executed and the queue is empty.
  //   * Downloading: A download is in progress, and additional requests may be
  //   in queue.
  enum class QueueState {
    kWaiting,
    kDownloading,
  };

 public:
  using ImageListUpdatedCallback =
      base::RepeatingCallback<void(const std::vector<base::FilePath>& images)>;

  ScreensaverImageDownloader() = delete;

  ScreensaverImageDownloader(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      const base::FilePath& download_directory,
      ImageListUpdatedCallback image_list_updated_callback);

  ~ScreensaverImageDownloader();

  ScreensaverImageDownloader(const ScreensaverImageDownloader&) = delete;
  ScreensaverImageDownloader& operator=(const ScreensaverImageDownloader&) =
      delete;

  // Updates the list of images to be cached to `image_url_list`. Processing the
  // new list can download new images and delete images that are no longer being
  // referenced in the new list.
  void UpdateImageUrlList(const base::Value::List& image_url_list);

  std::vector<base::FilePath> GetScreensaverImages();

  // Used for setting images in tests.
  void SetImagesForTesting(const std::vector<base::FilePath>& images);

  base::FilePath GetDowloadDirForTesting();

 private:
  friend class ScreensaverImageDownloaderTest;

  // Called when unreferenced images have been deleted. Used for removing stale
  // file references from the in-memory `downloaded_images_` cache.
  void OnUnreferencedImagesDeleted(
      std::vector<base::FilePath> file_paths_deleted);

  // Downloads a new external image from `image_url` to the download folder as
  // `file_name`. The async `callback` will pass the result, and the file path
  // if the operation succeeded.
  void QueueImageDownload(const std::string& image_url);

  // Empties the downloading queue, and replies to pending requests to indicate
  // that they have been cancelled.
  void ClearRequestQueue();

  // Clears out the download folder.
  void DeleteDownloadedImages();

  // Verifies that the download directory is present and writable, or attempts
  // to create it otherwise. The result of this operation is passed along to
  // `OnVerifyDownloadDirectoryCompleted`.
  void StartImageDownload(const std::string& image_url);

  // Starts a new download if the download folder is present and writable.
  // Otherwise, it completes the request with an error result.
  void OnVerifyDownloadDirectoryCompleted(const std::string& image_url,
                                          bool can_download_to_dir);

  // Resolves the download request if the file is already cached, otherwise
  // triggers a new URL request to download the file.
  void OnCheckIsFileIsInCache(const base::FilePath& file_path,
                              const std::string& image_url,
                              bool is_file_present);

  // Moves the downloaded image to its desired path. To avoid reading
  // errors, every image is initially downloaded to a temporary file. On
  // network error, `callback` is invoked.
  void OnUrlDownloadedToTempFile(
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      const std::string& image_url,
      base::FilePath temp_path);

  // Handles the final result of the image download process, and triggers the
  // complete `callback`.
  void OnUrlDownloadToFileComplete(const base::FilePath& path,
                                   const std::string& image_url,
                                   bool file_is_present);

  // Completes a download by calling `result` with `result` and `path`. It will
  // attempt to start the next download, if any.
  void FinishImageDownload(const std::string& image_url,
                           ScreensaverImageDownloadResult result,
                           std::optional<base::FilePath> path);

  QueueState queue_state_ = QueueState::kWaiting;

  // To avoid multiple URL requests, only one download can be executed.
  // Additional downloads will be queued, and executed sequentially.
  base::queue<std::string> downloading_queue_;

  base::flat_set<base::FilePath> downloaded_images_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  base::FilePath download_directory_;

  // Used to notify changes in the list of downloaded images.
  ImageListUpdatedCallback image_list_updated_callback_;

  base::WeakPtrFactory<ScreensaverImageDownloader> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_MANAGED_SCREENSAVER_IMAGE_DOWNLOADER_H_
