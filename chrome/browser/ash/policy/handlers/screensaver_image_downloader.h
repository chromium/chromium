// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREENSAVER_IMAGE_DOWNLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREENSAVER_IMAGE_DOWNLOADER_H_

#include <memory>
#include <string>

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

namespace policy {

enum class ScreensaverImageDownloadResult {
  kSuccess,
  kNetworkError,
  kFileSaveError,
  kFileSystemWriteError,
};

// Provides a service to download external image files that will be displayed in
// the managed screensaver feature.
class ScreensaverImageDownloader {
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
  // Convenience definition for the callback provided by clients wanting to
  // download images.
  using ResultCallback =
      base::OnceCallback<void(ScreensaverImageDownloadResult result,
                              absl::optional<base::FilePath> path)>;

  // Represents a single image download request from `image_url` to
  // `download_directory_` with name `file_name`. Once this job has been
  // completed, `result_callback` will be invoked with the actual result, and
  // the path to the downloaded file if the operation suceeded.
  struct Job {
    Job() = delete;
    Job(const std::string& image_url,
        const std::string& file_name,
        ResultCallback result_callback);
    ~Job();

    const std::string image_url;
    std::string file_name;
    ResultCallback result_callback;
  };

  ScreensaverImageDownloader() = delete;

  ScreensaverImageDownloader(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      const base::FilePath& download_directory);

  ~ScreensaverImageDownloader();

  ScreensaverImageDownloader(const ScreensaverImageDownloader&) = delete;
  ScreensaverImageDownloader& operator=(const ScreensaverImageDownloader&) =
      delete;

  // Downloads a new external image from `image_url` to the download folder as
  // `file_name`. The async `callback` will pass the result, and the file path
  // if the operation succeeded.
  void QueueDownloadJob(std::unique_ptr<Job> download_job);

 private:
  friend class ScreensaverImageDownloaderTest;

  // Verifies that the download directory is present and writable, or attempts
  // to create it otherwise. The result of this operation is passed along to
  // `StartDownloadJobInternal`.
  void StartDownloadJob(std::unique_ptr<Job> download_job);

  // Triggers a new URL request to download an image, if `can_download_file` is
  // true. Otherwise, it completes the job with an error result.
  void StartDownloadJobInternal(std::unique_ptr<Job> download_job,
                                bool can_download_file);

  // Moves the downloaded image to its desired path. To avoid reading errors,
  // every image is initially downloaded to a temporary file. On network error,
  // `callback` is invoked.
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

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  base::FilePath download_directory_;

  base::WeakPtrFactory<ScreensaverImageDownloader> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREENSAVER_IMAGE_DOWNLOADER_H_
