// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/managed/screensaver_image_downloader.h"

#include <string>

#include "ash/ambient/metrics/managed_screensaver_metrics.h"
#include "base/containers/flat_set.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash {

namespace {

constexpr net::NetworkTrafficAnnotationTag
    kScreensaverImageDownloaderNetworkTag =
        net::DefineNetworkTrafficAnnotation("screensaver_image_downloader",
                                            R"(
        semantics {
          sender: "Managed Screensaver"
          description:
            "Fetch external image files that will be cached and displayed "
            "in the policy-controlled screensaver."
          trigger:
            "An update to the ScreensaverLockScreenImages policy that includes "
            "new references to external image files."
          data:
            "This request does not send any data from the device. It fetches"
            "images from URLs provided by the policy."
          destination: OTHER
          user_data {
            type: NONE
          }
          internal {
            contacts {
              email: "mpetrisor@google.com"
            }
          }
          last_reviewed: "2023-03-30"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is controlled by enterprise policies, and cannot"
            "be overridden by users. It is disabled by default."
          chrome_policy {
            ScreensaverLockScreenImages {
              ScreensaverLockScreenImages {
                  entries: ""
              }
            }
          }
        })");
constexpr char kCacheFileExt[] = ".cache";
constexpr char kCacheFileWildCardPattern[] = "*.cache";

constexpr int64_t kMaxFileSizeInBytes = 8 * 1024 * 1024;  // 8 MB
constexpr int kMaxUrlFetchRetries = 3;

// This limit is specified in the policy definition for the policies
// ScreensaverLockScreenImages and DeviceScreensaverLoginScreenImages.
constexpr size_t kMaxUrlsToProcessFromPolicy = 25u;

std::unique_ptr<network::SimpleURLLoader> CreateSimpleURLLoader(
    const std::string& url) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(url);
  request->method = net::HttpRequestHeaders::kGetMethod;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  CHECK(request->url.SchemeIs(url::kHttpsScheme));

  auto loader = network::SimpleURLLoader::Create(
      std::move(request), kScreensaverImageDownloaderNetworkTag);
  const int retry_mode = network::SimpleURLLoader::RETRY_ON_5XX |
                         network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE;
  loader->SetRetryOptions(kMaxUrlFetchRetries, retry_mode);
  return loader;
}

// Helper function to extract response code from `SimpleURLLoader`.
int GetResponseCode(network::SimpleURLLoader* simple_loader) {
  if (!simple_loader->ResponseInfo() ||
      !simple_loader->ResponseInfo()->headers) {
    return -1;
  }
  return simple_loader->ResponseInfo()->headers->response_code();
}

bool VerifyOrCreateDownloadDirectory(const base::FilePath& download_directory) {
  if (!base::DirectoryExists(download_directory) &&
      !base::CreateDirectory(download_directory)) {
    LOG(ERROR) << "Cannot create download directory";
    // TODO(b/276208772): Track result with metrics
    return false;
  }
  if (!base::PathIsWritable(download_directory)) {
    LOG(ERROR) << "Cannot write to download directory";
    // TODO(b/276208772): Track result with metrics
    return false;
  }
  return true;
}

std::string GetHashedFileNameForUrl(const std::string& url) {
  auto hash = base::SHA1Hash(base::as_byte_span(url));
  return base::HexEncode(hash) + kCacheFileExt;
}

std::vector<std::string> GetImageUrlsToProcess(
    const base::Value::List& image_url_list) {
  std::vector<std::string> urls;
  for (size_t i = 0;
       i < kMaxUrlsToProcessFromPolicy && i < image_url_list.size(); ++i) {
    const base::Value& value = image_url_list[i];
    if (!value.is_string() || value.GetString().empty()) {
      continue;
    }
    // Canonicalize URLs and require HTTPS.
    GURL url(value.GetString());
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
      LOG(WARNING) << "Ignored invalid URL: " << url;
      continue;
    }

    urls.emplace_back(url.spec());
  }
  return urls;
}

// Returns all the cached images in the provided directory.
// This method does blocking IO and should only be run on a thread that
// allows blocking IO.
std::vector<base::FilePath> GetCachedImagesFromDisk(
    const base::FilePath& directory) {
  std::vector<base::FilePath> images_on_disk;
  base::FileEnumerator iterator(directory, /*recursive=*/false,
                                base::FileEnumerator::FILES,
                                FILE_PATH_LITERAL(kCacheFileWildCardPattern));
  base::FilePath current_path;
  for (base::FilePath path = iterator.Next(); !path.empty();
       path = iterator.Next()) {
    images_on_disk.push_back(path);
  }

  return images_on_disk;
}

// Deletes all the provided files in the `files_to_delete` parameter.
// This method does blocking IO and should only be run on a thread that
// allows blocking IO.
// Note: In case all of the files are successfully deleted, this will return
// true otherwise will return false.
bool DeleteFiles(const std::vector<base::FilePath>& files_to_delete) {
  bool success = true;
  for (const auto& path : files_to_delete) {
    // Even if one file fails to delete mark this operation as not being
    // success.
    if (!base::DeleteFile(path)) {
      LOG(WARNING) << "Failed to clean up: " << path.BaseName().value();
      success = false;
    }
  }
  return success;
}

// Deletes all images on disk in the cache directory that are not referenced
// by the given `new_image_urls`.
std::vector<base::FilePath> DeleteUnreferencedImageFiles(
    const std::vector<std::string>& new_image_urls,
    const base::FilePath& download_directory) {
  std::vector<std::string> hashed_image_urls = new_image_urls;
  // Hash the image url
  base::ranges::transform(hashed_image_urls.begin(), hashed_image_urls.end(),
                          hashed_image_urls.begin(), GetHashedFileNameForUrl);

  base::flat_set<std::string> hashed_image_file_paths(hashed_image_urls);

  auto cached_images_from_disk = GetCachedImagesFromDisk(download_directory);

  std::vector<base::FilePath> file_paths_to_delete;
  for (const auto& downloaded_file : cached_images_from_disk) {
    if (!hashed_image_file_paths.contains(downloaded_file.BaseName().value())) {
      file_paths_to_delete.push_back(downloaded_file);
    }
  }
  if (!DeleteFiles(file_paths_to_delete)) {
    // TODO(b/276208772): Track result with metrics
    DLOG(WARNING) << "Failed to delete some of the files";
  }

  return file_paths_to_delete;
}

}  // namespace

ScreensaverImageDownloader::ScreensaverImageDownloader(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    const base::FilePath& download_directory,
    ImageListUpdatedCallback image_list_updated_callback)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      shared_url_loader_factory_(shared_url_loader_factory),
      download_directory_(download_directory),
      image_list_updated_callback_(image_list_updated_callback) {}

ScreensaverImageDownloader::~ScreensaverImageDownloader() = default;

void ScreensaverImageDownloader::UpdateImageUrlList(
    const base::Value::List& image_url_list) {
  if (image_url_list.empty()) {
    // If the screensaver is listening to updates, notify that the images are no
    // longer available before deleting them.
    image_list_updated_callback_.Run(std::vector<base::FilePath>());

    ClearRequestQueue();
    weak_ptr_factory_.InvalidateWeakPtrs();
    DeleteDownloadedImages();
    downloaded_images_.clear();
    return;
  }

  const std::vector<std::string> new_image_urls =
      GetImageUrlsToProcess(image_url_list);

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DeleteUnreferencedImageFiles, new_image_urls,
                     download_directory_),
      base::BindOnce(&ScreensaverImageDownloader::OnUnreferencedImagesDeleted,
                     weak_ptr_factory_.GetWeakPtr()));

  for (const std::string& image_url : new_image_urls) {
    DVLOG(1) << "Queue URL: " << image_url;
    QueueImageDownload(image_url);
  }
}

void ScreensaverImageDownloader::OnUnreferencedImagesDeleted(
    std::vector<base::FilePath> file_paths_deleted) {
  if (file_paths_deleted.empty()) {
    return;
  }
  for (const auto& path : file_paths_deleted) {
    downloaded_images_.erase(path);
    DVLOG(1) << "Removing path from in memory cache " << path;
  }

  image_list_updated_callback_.Run(std::vector<base::FilePath>(
      downloaded_images_.begin(), downloaded_images_.end()));
}

std::vector<base::FilePath> ScreensaverImageDownloader::GetScreensaverImages() {
  return std::vector<base::FilePath>(downloaded_images_.begin(),
                                     downloaded_images_.end());
}

void ScreensaverImageDownloader::SetImagesForTesting(
    const std::vector<base::FilePath>& images_file_paths) {
  downloaded_images_ = base::flat_set<base::FilePath>(images_file_paths);
}

base::FilePath ScreensaverImageDownloader::GetDowloadDirForTesting() {
  return download_directory_;
}

void ScreensaverImageDownloader::QueueImageDownload(
    const std::string& image_url) {
  // TODO(b/276208772): Track queue usage with metrics
  if (queue_state_ == QueueState::kWaiting) {
    CHECK(downloading_queue_.empty());
    StartImageDownload(image_url);
  } else {
    downloading_queue_.emplace(image_url);
  }
}

void ScreensaverImageDownloader::ClearRequestQueue() {
  base::queue<std::string> buffer_queue;
  buffer_queue.swap(downloading_queue_);
  queue_state_ = QueueState::kWaiting;

  while (!buffer_queue.empty()) {
    FinishImageDownload(buffer_queue.front(),
                        ScreensaverImageDownloadResult::kCancelled,
                        std::nullopt);
    buffer_queue.pop();
  }
}

void ScreensaverImageDownloader::DeleteDownloadedImages() {
  // TODO(b/278548884): Do not ignore callback result and track its result.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&base::DeletePathRecursively),
                     download_directory_));
}

void ScreensaverImageDownloader::StartImageDownload(
    const std::string& image_url) {
  queue_state_ = QueueState::kDownloading;
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&VerifyOrCreateDownloadDirectory, download_directory_),
      base::BindOnce(
          &ScreensaverImageDownloader::OnVerifyDownloadDirectoryCompleted,
          weak_ptr_factory_.GetWeakPtr(), image_url));
}

void ScreensaverImageDownloader::OnVerifyDownloadDirectoryCompleted(
    const std::string& image_url,
    bool can_download_file) {
  if (!can_download_file) {
    FinishImageDownload(image_url,
                        ScreensaverImageDownloadResult::kFileSystemWriteError,
                        std::nullopt);
    return;
  }

  // The download folder exists, check if the file is already in cache before
  // attempting to download it.
  const base::FilePath file_path =
      download_directory_.AppendASCII(GetHashedFileNameForUrl(image_url));
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PathExists, file_path),
      base::BindOnce(&ScreensaverImageDownloader::OnCheckIsFileIsInCache,
                     weak_ptr_factory_.GetWeakPtr(), file_path, image_url));
}

void ScreensaverImageDownloader::OnCheckIsFileIsInCache(
    const base::FilePath& file_path,
    const std::string& image_url,
    bool is_file_present) {
  if (is_file_present) {
    FinishImageDownload(image_url, ScreensaverImageDownloadResult::kSuccess,
                        file_path);
    return;
  }

  CHECK(shared_url_loader_factory_);
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      CreateSimpleURLLoader(image_url);

  auto* loader = simple_loader.get();
  // Download to temp file first to guarantee entire image is written without
  // errors before attempting to read it.
  loader->DownloadToTempFile(
      shared_url_loader_factory_.get(),
      base::BindOnce(&ScreensaverImageDownloader::OnUrlDownloadedToTempFile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(simple_loader),
                     image_url),
      kMaxFileSizeInBytes);
}

void ScreensaverImageDownloader::OnUrlDownloadedToTempFile(
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    const std::string& image_url,
    base::FilePath temp_path) {
  const base::FilePath desired_path =
      download_directory_.AppendASCII(GetHashedFileNameForUrl(image_url));
  if (simple_loader->NetError() != net::OK || temp_path.empty()) {
    LOG(ERROR) << "Downloading to file failed with error code: "
               << GetResponseCode(simple_loader.get()) << " with network error "
               << simple_loader->NetError();

    if (!temp_path.empty()) {
      // Clean up temporary file.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(base::IgnoreResult(&base::DeleteFile), temp_path));
    }
    FinishImageDownload(
        image_url, ScreensaverImageDownloadResult::kNetworkError, std::nullopt);
    return;
  }

  // Swap the temporary file to the desired path, and then run the callback.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::Move, temp_path, desired_path),
      base::BindOnce(&ScreensaverImageDownloader::OnUrlDownloadToFileComplete,
                     weak_ptr_factory_.GetWeakPtr(), desired_path, image_url));
}

void ScreensaverImageDownloader::OnUrlDownloadToFileComplete(
    const base::FilePath& path,
    const std::string& image_url,
    bool file_is_present) {
  if (!file_is_present) {
    DLOG(WARNING) << "Could not save the downloaded file to " << path;
    FinishImageDownload(image_url,
                        ScreensaverImageDownloadResult::kFileSaveError,
                        std::nullopt);
    return;
  }

  FinishImageDownload(image_url, ScreensaverImageDownloadResult::kSuccess,
                      path);
}

void ScreensaverImageDownloader::FinishImageDownload(
    const std::string& image_url,
    ScreensaverImageDownloadResult result,
    std::optional<base::FilePath> path) {
  RecordManagedScreensaverImageDownloadResult(result);

  if (result == ScreensaverImageDownloadResult::kSuccess) {
    downloaded_images_.insert(*path);
    image_list_updated_callback_.Run(std::vector<base::FilePath>(
        downloaded_images_.begin(), downloaded_images_.end()));
  }

  if (downloading_queue_.empty()) {
    queue_state_ = QueueState::kWaiting;
  } else {
    StartImageDownload(std::move(downloading_queue_.front()));
    downloading_queue_.pop();
  }
}

}  // namespace ash
