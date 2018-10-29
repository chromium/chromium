// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/image_thumbnail_request.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/download/thumbnail_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"

namespace {

// Ignore image files that are too large to avoid long delays.
const int64_t kMaxImageSize = 10 * 1024 * 1024;  // 10 MB

std::string LoadImageData(const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::WILL_BLOCK);

  // Confirm that the file's size is within our threshold.
  int64_t file_size;
  if (!base::GetFileSize(path, &file_size) || file_size > kMaxImageSize) {
    LOG(ERROR) << "Unexpected file size: " << path.MaybeAsASCII() << ", "
               << file_size;
    return std::string();
  }

  std::string data;
  bool success = base::ReadFileToString(path, &data);

  // Make sure the file isn't empty.
  if (!success || data.empty()) {
    LOG(ERROR) << "Failed to read file: " << path.MaybeAsASCII();
    return std::string();
  }

  return data;
}

}  // namespace

ImageThumbnailRequest::ImageThumbnailRequest(
    int icon_size,
    base::OnceCallback<void(const SkBitmap&)> callback)
    : icon_size_(icon_size),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ImageThumbnailRequest::~ImageThumbnailRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ImageThumbnailRequest::Start(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&LoadImageData, path),
      base::BindOnce(&ImageThumbnailRequest::OnLoadComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageThumbnailRequest::OnImageDecoded(const SkBitmap& decoded_image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ScaleDownBitmap(icon_size_, decoded_image,
                  base::BindOnce(&ImageThumbnailRequest::FinishRequest,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void ImageThumbnailRequest::OnDecodeImageFailed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LOG(ERROR) << "Failed to decode image.";
  FinishRequest(SkBitmap());
}

void ImageThumbnailRequest::OnLoadComplete(const std::string& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (data.empty()) {
    FinishRequest(SkBitmap());
    return;
  }

  ImageDecoder::Start(this, data);
}

void ImageThumbnailRequest::FinishRequest(SkBitmap thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(std::move(callback_), std::move(thumbnail)));
  delete this;
}
