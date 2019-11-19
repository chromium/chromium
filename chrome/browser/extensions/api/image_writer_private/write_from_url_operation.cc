// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/write_from_url_operation.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace extensions {
namespace image_writer {

using content::BrowserThread;

WriteFromUrlOperation::WriteFromUrlOperation(
    base::WeakPtr<OperationManager> manager,
    const ExtensionId& extension_id,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote,
    GURL url,
    const std::string& hash,
    const std::string& device_path,
    const base::FilePath& download_folder)
    : Operation(manager, extension_id, device_path, download_folder),
      url_loader_factory_remote_(std::move(factory_remote)),
      url_(url),
      hash_(hash),
      download_continuation_() {}

WriteFromUrlOperation::~WriteFromUrlOperation() = default;

void WriteFromUrlOperation::StartImpl() {
  DCHECK(IsRunningInCorrectSequence());

  GetDownloadTarget(base::BindOnce(
      &WriteFromUrlOperation::Download, this,
      base::BindOnce(
          &WriteFromUrlOperation::VerifyDownload, this,
          base::BindOnce(
              &WriteFromUrlOperation::Unzip, this,
              base::Bind(&WriteFromUrlOperation::Write, this,
                         base::Bind(&WriteFromUrlOperation::VerifyWrite, this,
                                    base::Bind(&WriteFromUrlOperation::Finish,
                                               this)))))));
}

void WriteFromUrlOperation::GetDownloadTarget(base::OnceClosure continuation) {
  DCHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    return;
  }

  if (url_.ExtractFileName().empty()) {
    if (!base::CreateTemporaryFileInDir(temp_dir_->GetPath(), &image_path_)) {
      Error(error::kTempFileError);
      return;
    }
  } else {
    base::FilePath file_name =
        base::FilePath::FromUTF8Unsafe(url_.ExtractFileName());
    image_path_ = temp_dir_->GetPath().Append(file_name);
  }

  PostTask(std::move(continuation));
}

void WriteFromUrlOperation::Download(base::OnceClosure continuation) {
  DCHECK(IsRunningInCorrectSequence());

  if (IsCancelled()) {
    return;
  }

  download_continuation_ = std::move(continuation);

  SetStage(image_writer_api::STAGE_DOWNLOAD);

  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("cros_recovery_image_download", R"(
        semantics {
          sender: "Chrome OS Recovery Utility"
          description:
            "The Google Chrome OS recovery utility downloads the recovery "
            "image from Google Download Server."
          trigger:
            "User uses the Chrome OS Recovery Utility app/extension, selects "
            "a Chrome OS recovery image, and clicks the Create button to write "
            "the image to a USB or SD card."
          data:
            "URL of the image file to be downloaded. No other data or user "
            "identifier is sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled by settings, it can only be used "
            "by whitelisted apps/extension."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(url_);
  simple_url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  simple_url_loader_->SetOnDownloadProgressCallback(base::BindRepeating(
      &WriteFromUrlOperation::OnDataDownloaded, base::Unretained(this)));
  simple_url_loader_->SetOnResponseStartedCallback(base::BindOnce(
      &WriteFromUrlOperation::OnResponseStarted, base::Unretained(this)));

  AddCleanUpFunction(
      base::BindOnce(&WriteFromUrlOperation::DestroySimpleURLLoader, this));

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_remote(
      std::move(url_loader_factory_remote_));

  simple_url_loader_->DownloadToFile(
      url_loader_factory_remote.get(),
      base::BindOnce(&WriteFromUrlOperation::OnSimpleLoaderComplete,
                     base::Unretained(this)),
      image_path_);
}

void WriteFromUrlOperation::DestroySimpleURLLoader() {
  simple_url_loader_.reset();
}

void WriteFromUrlOperation::OnResponseStarted(
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  total_response_bytes_ = response_head.content_length;
}

void WriteFromUrlOperation::OnDataDownloaded(uint64_t current) {
  DCHECK(IsRunningInCorrectSequence());

  if (IsCancelled())
    DestroySimpleURLLoader();

  int progress = (kProgressComplete * current) / total_response_bytes_;

  SetProgress(progress);
}

void WriteFromUrlOperation::OnSimpleLoaderComplete(base::FilePath file_path) {
  DCHECK(IsRunningInCorrectSequence());
  if (!file_path.empty()) {
    SetProgress(kProgressComplete);

    std::move(download_continuation_).Run();
  } else {
    Error(error::kDownloadInterrupted);
  }
}

void WriteFromUrlOperation::VerifyDownload(base::OnceClosure continuation) {
  DCHECK(IsRunningInCorrectSequence());

  if (IsCancelled()) {
    return;
  }

  // Skip verify if no hash.
  if (hash_.empty()) {
    PostTask(std::move(continuation));
    return;
  }

  SetStage(image_writer_api::STAGE_VERIFYDOWNLOAD);

  GetMD5SumOfFile(image_path_, 0, 0, kProgressComplete,
                  base::BindOnce(&WriteFromUrlOperation::VerifyDownloadCompare,
                                 this, std::move(continuation)));
}

void WriteFromUrlOperation::VerifyDownloadCompare(
    base::OnceClosure continuation,
    const std::string& download_hash) {
  DCHECK(IsRunningInCorrectSequence());
  if (download_hash != hash_) {
    Error(error::kDownloadHashError);
    return;
  }

  PostTask(base::BindOnce(&WriteFromUrlOperation::VerifyDownloadComplete, this,
                          std::move(continuation)));
}

void WriteFromUrlOperation::VerifyDownloadComplete(
    base::OnceClosure continuation) {
  DCHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    return;
  }

  SetProgress(kProgressComplete);
  PostTask(std::move(continuation));
}

} // namespace image_writer
} // namespace extensions
