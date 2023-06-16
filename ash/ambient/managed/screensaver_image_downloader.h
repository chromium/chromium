// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MANAGED_SCREENSAVER_IMAGE_DOWNLOADER_H_
#define ASH_AMBIENT_MANAGED_SCREENSAVER_IMAGE_DOWNLOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  // Expresses the state of the downloading job queue. It only has two possible
  // states:
  //   * Waiting: No job is being executed and the queue is empty.
  //   * Downloading: A job is in progress, and additional jobs may be in queue.
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

  // Represents a single image download request from `image_url` to
  // `download_directory_`. Once this job has been completed, `result_callback`
  // will be invoked with the actual result, and the path to the downloaded file
  // if the operation succeeded.
  // TODO(b/280810255): Delete this class, and use a plain std::string.
  struct Job {
    Job() = delete;
    explicit Job(const std::string& image_url);
    ~Job();

    // Creates a unique name based on a hash operation on the image URL to
    // be used for the file stored in disk.
    std::string file_name() const;

    const std::string image_url;
  };

  // Deletes all images on disk in the cache directory that are not referenced
  // by the given `new_image_urls`.
  std::vector<base::FilePath> DeleteUnreferencedImageFiles(
      const std::vector<std::string>& new_image_urls);

  // Called when unreferenced images have been deleted. Used for removing stale
  // file references from the in-memory `downloaded_images_` cache.
  void OnUnreferencedImagesDeleted(
      std::vector<base::FilePath> file_paths_deleted);

  // Downloads a new external image from `image_url` to the download folder as
  // `file_name`. The async `callback` will pass the result, and the file path
  // if the operation succeeded.
  void QueueDownloadJob(std::unique_ptr<Job> download_job);

  // Empties the downloading queue, and replies to pending requests to indicate
  // that they have been cancelled.
  void ClearRequestQueue();

  // Clears out the download folder.
  void DeleteDownloadedImages();

  // Verifies that the download directory is present and writable, or attempts
  // to create it otherwise. The result of this operation is passed along to
  // `OnVerifyDownloadDirectoryCompleted`.
  void StartDownloadJob(std::unique_ptr<Job> download_job);

  // Starts a new job if the download folder is present and writable.
  // Otherwise, it completes the request with an error result.
  void OnVerifyDownloadDirectoryCompleted(std::unique_ptr<Job> download_job,
                                          bool can_download_to_dir);

  // Resolves the download request if the file is already cached, otherwise
  // triggers a new URL request to download the file.
  void OnCheckIsFileIsInCache(const base::FilePath& file_path,
                              std::unique_ptr<Job> download_job,
                              bool is_file_present);

  // Moves the downloaded image to its desired path. To avoid reading
  // errors, every image is initially downloaded to a temporary file. On
  // network error, `callback` is invoked.
  void OnUrlDownloadedToTempFile(
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      std::unique_ptr<Job> download_job,
      base::FilePath temp_path);

  // Handles the final result of the image download process, and triggers the
  // complete `callback`.
  void OnUrlDownloadToFileComplete(const base::FilePath& path,
                                   std::unique_ptr<Job> download_job,
                                   bool file_is_present);

  // Completes a job by calling `result` with `result` and `path`. It will
  // attempt to start the next pending job, if there is any.
  void FinishDownloadJob(std::unique_ptr<Job> download_job,
                         ScreensaverImageDownloadResult result,
                         absl::optional<base::FilePath> path);

  QueueState queue_state_ = QueueState::kWaiting;

  // To avoid multiple URL requests, only one job can be executed. Additional
  // jobs will be queued, and executed sequentially.
  base::queue<std::unique_ptr<Job>> downloading_queue_;

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
