// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_item_request.h"

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

std::string GetFileContentsBlocking(base::FilePath path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return "";

  int64_t file_size = file.GetLength();
  std::string contents;
  contents.resize(file_size);

  int64_t bytes_read = 0;
  while (bytes_read < file_size) {
    int64_t bytes_currently_read =
        file.ReadAtCurrentPos(&contents[bytes_read], file_size - bytes_read);
    if (bytes_currently_read == -1)
      return "";

    bytes_read += bytes_currently_read;
  }

  return contents;
}

}  // namespace

DownloadItemRequest::DownloadItemRequest(download::DownloadItem* item,
                                         bool read_immediately,
                                         BinaryUploadService::Callback callback)
    : Request(std::move(callback)), item_(item), weakptr_factory_(this) {
  if (read_immediately)
    ReadFile();

  item_->AddObserver(this);
}

DownloadItemRequest::~DownloadItemRequest() {
  if (item_ != nullptr)
    item_->RemoveObserver(this);
}

void DownloadItemRequest::GetRequestData(DataCallback callback) {
  if (item_ == nullptr) {
    std::move(callback).Run(BinaryUploadService::Result::UNKNOWN, Data());
    return;
  }

  if (static_cast<size_t>(item_->GetTotalBytes()) >
      BinaryUploadService::kMaxUploadSizeBytes) {
    std::move(callback).Run(BinaryUploadService::Result::FILE_TOO_LARGE,
                            Data());
    return;
  }

  if (is_data_valid_) {
    std::move(callback).Run(BinaryUploadService::Result::SUCCESS, data_);
    return;
  }

  pending_callbacks_.push_back(std::move(callback));
}

void DownloadItemRequest::RunPendingGetFileContentsCallbacks() {
  for (auto it = pending_callbacks_.begin(); it != pending_callbacks_.end();
       it++) {
    std::move(*it).Run(BinaryUploadService::Result::SUCCESS, data_);
  }

  pending_callbacks_.clear();
}

void DownloadItemRequest::OnDownloadUpdated(download::DownloadItem* download) {
  if (!is_data_valid_ && download == item_ &&
      item_->GetFullPath() == item_->GetTargetFilePath())
    ReadFile();
}

void DownloadItemRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  if (download == item_)
    item_ = nullptr;
}

void DownloadItemRequest::ReadFile() {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetFileContentsBlocking, item_->GetFullPath()),
      base::BindOnce(&DownloadItemRequest::OnGotFileContents,
                     weakptr_factory_.GetWeakPtr()));
}

void DownloadItemRequest::OnGotFileContents(std::string contents) {
  data_.contents = std::move(contents);
  is_data_valid_ = true;
  RunPendingGetFileContentsCallbacks();
}

}  // namespace safe_browsing
