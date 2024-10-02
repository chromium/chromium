// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/generator/image_thumbnail_request.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/thumbnail/generator/thumbnail_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"

namespace {

// Ignore image files that are too large to avoid long delays.
const int64_t kMaxImageSize = 10 * 1024 * 1024;  // 10 MB

std::vector<uint8_t> LoadImageData(const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  std::vector<uint8_t> data;
  // Confirm that the file's size is within our threshold.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!file.IsValid())
    return data;

  int64_t file_size = file.GetLength();
  if (file_size <= 0 || file_size > kMaxImageSize) {
    LOG(ERROR) << "Unexpected file size: " << path.MaybeAsASCII() << ", "
               << file_size;
    return data;
  }

  data.resize(file_size);
  if (!file.ReadAndCheck(0, data)) {
    LOG(ERROR) << "Failed to read file: " << path.MaybeAsASCII();
    data.clear();
  }

  return data;
}

}  // namespace

ImageThumbnailRequest::ImageThumbnailRequest(
    int icon_size,
    base::OnceCallback<void(const SkBitmap&)> callback)
    : icon_size_(icon_size), callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ImageThumbnailRequest::~ImageThumbnailRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ImageThumbnailRequest::Start(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
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

void ImageThumbnailRequest::OnLoadComplete(std::vector<uint8_t> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (data.empty()) {
    FinishRequest(SkBitmap());
    return;
  }

  ImageDecoder::Start(this, std::move(data));
}

void ImageThumbnailRequest::FinishRequest(SkBitmap thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(thumbnail)));
  delete this;
}
