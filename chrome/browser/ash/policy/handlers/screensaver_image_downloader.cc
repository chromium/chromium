// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screensaver_image_downloader.h"

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace policy {

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
            contacts {
              email: "eariassoto@google.com"
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
            subProto1 {
              ScreensaverLockScreenImages {
                ScreensaverLockScreenImages {
                    entries: ""
                }
              }
            }
          }
        })");

constexpr int64_t kMaxFileSizeInBytes = 8 * 1024 * 1024;  // 8 MB
constexpr int kMaxUrlFetchRetries = 3;

std::unique_ptr<network::SimpleURLLoader> CreateSimpleURLLoader(
    const std::string& url) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(url);
  request->method = net::HttpRequestHeaders::kGetMethod;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

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
}  // namespace

ScreensaverImageDownloader::ScreensaverImageDownloader(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    const base::FilePath& download_directory)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      shared_url_loader_factory_(shared_url_loader_factory),
      download_directory_(download_directory) {}

ScreensaverImageDownloader::~ScreensaverImageDownloader() = default;

void ScreensaverImageDownloader::DowloadImageFromUrl(
    const std::string& image_url,
    const std::string& file_name,
    ResultCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path) {
            if (!base::DirectoryExists(path) && !base::CreateDirectory(path)) {
              LOG(ERROR) << "Cannot create download directory";
              // TODO(b/276208772): Track result with metrics
              return false;
            }
            if (!base::PathIsWritable(path)) {
              LOG(ERROR) << "Cannot write to download directory";
              // TODO(b/276208772): Track result with metrics
              return false;
            }
            return true;
          },
          download_directory_),
      base::BindOnce(&ScreensaverImageDownloader::DownloadImageToFileInternal,
                     weak_ptr_factory_.GetWeakPtr(), image_url, file_name,
                     std::move(callback)));
}

void ScreensaverImageDownloader::DownloadImageToFileInternal(
    const std::string& url,
    const std::string& file_name,
    ResultCallback callback,
    bool can_download_file) {
  if (!can_download_file) {
    // TODO(b/276208772): Track result with metrics
    std::move(callback).Run(
        ScreensaverImageDownloadResult::kFileSystemWriteError, absl::nullopt);
    return;
  }

  CHECK(shared_url_loader_factory_);
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      CreateSimpleURLLoader(url);

  auto* loader = simple_loader.get();
  // Download to temp file first to guarantee entire image is written without
  // errors before attempting to read it.
  loader->DownloadToTempFile(
      shared_url_loader_factory_.get(),
      base::BindOnce(&ScreensaverImageDownloader::OnUrlDownloadedToTempFile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(simple_loader),
                     file_name, std::move(callback)),
      kMaxFileSizeInBytes);
}

void ScreensaverImageDownloader::OnUrlDownloadedToTempFile(
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    const std::string& file_name,
    ResultCallback callback,
    base::FilePath temp_path) {
  const base::FilePath desired_path =
      download_directory_.AppendASCII(file_name);
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
    // TODO(b/276208772): Track result with metrics
    std::move(callback).Run(ScreensaverImageDownloadResult::kNetworkError,
                            absl::nullopt);
    return;
  }

  // Swap the temporary file to the desired path, and then run the callback.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::Move, temp_path, desired_path),
      base::BindOnce(&ScreensaverImageDownloader::OnUrlDownloadToFileComplete,
                     weak_ptr_factory_.GetWeakPtr(), desired_path,
                     std::move(callback)));
}

void ScreensaverImageDownloader::OnUrlDownloadToFileComplete(
    const base::FilePath& path,
    ResultCallback callback,
    bool file_is_present) {
  // TODO(b/276208772): Track result with metrics
  if (!file_is_present) {
    DLOG(WARNING) << "Could not save the downloaded file to " << path;
    std::move(callback).Run(ScreensaverImageDownloadResult::kFileSaveError,
                            absl::nullopt);
    return;
  }

  std::move(callback).Run(ScreensaverImageDownloadResult::kSuccess, path);
}

}  // namespace policy
