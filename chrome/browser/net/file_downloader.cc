// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/file_downloader.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

const int kNumFileDownloaderRetries = 1;

FileDownloader::FileDownloader(
    const GURL& url,
    const base::FilePath& path,
    bool overwrite,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DownloadFinishedCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : url_loader_factory_(url_loader_factory),
      callback_(std::move(callback)),
      local_path_(path) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->SetRetryOptions(
      kNumFileDownloaderRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  if (overwrite) {
    simple_url_loader_->DownloadToTempFile(
        url_loader_factory_.get(),
        base::BindOnce(&FileDownloader::OnSimpleDownloadComplete,
                       base::Unretained(this)));
  } else {
    base::PostTaskAndReplyWithResult(
        base::CreateTaskRunner(
            {base::ThreadPool(), base::MayBlock(),
             base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
            .get(),
        FROM_HERE, base::Bind(&base::PathExists, local_path_),
        base::Bind(&FileDownloader::OnFileExistsCheckDone,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

FileDownloader::~FileDownloader() {}

void FileDownloader::OnSimpleDownloadComplete(base::FilePath response_path) {
  if (response_path.empty()) {
    if (simple_url_loader_->ResponseInfo() &&
        simple_url_loader_->ResponseInfo()->headers) {
      int response_code =
          simple_url_loader_->ResponseInfo()->headers->response_code();
      DLOG(WARNING) << "HTTP error " << response_code
                    << " while trying to download "
                    << simple_url_loader_->GetFinalURL().spec();
    }
    std::move(callback_).Run(FAILED);
    return;
  }

  base::PostTaskAndReplyWithResult(
      base::CreateTaskRunner({base::ThreadPool(), base::MayBlock(),
                              base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
          .get(),
      FROM_HERE, base::Bind(&base::Move, response_path, local_path_),
      base::Bind(&FileDownloader::OnFileMoveDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FileDownloader::OnFileExistsCheckDone(bool exists) {
  if (exists) {
    std::move(callback_).Run(EXISTS);
  } else {
    simple_url_loader_->DownloadToTempFile(
        url_loader_factory_.get(),
        base::BindOnce(&FileDownloader::OnSimpleDownloadComplete,
                       base::Unretained(this)));
  }
}

void FileDownloader::OnFileMoveDone(bool success) {
  if (!success) {
    DLOG(WARNING) << "Could not move file to "
                  << local_path_.LossyDisplayName();
  }

  std::move(callback_).Run(success ? DOWNLOADED : FAILED);
}
