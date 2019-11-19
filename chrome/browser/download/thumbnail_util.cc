// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/thumbnail_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"

namespace {

SkBitmap ScaleDownBitmapOnIOThread(int icon_size, SkBitmap bitmap) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (bitmap.drawsNothing())
    return bitmap;

  // Shrink the image down so that its smallest dimension is equal to or
  // smaller than the requested size.
  int min_dimension = std::min(bitmap.width(), bitmap.height());

  if (min_dimension <= icon_size)
    return bitmap;

  uint64_t width = static_cast<uint64_t>(bitmap.width());
  uint64_t height = static_cast<uint64_t>(bitmap.height());
  return skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BEST,
      width * icon_size / min_dimension, height * icon_size / min_dimension);
}

}  // namespace

void ScaleDownBitmap(int icon_size,
                     const SkBitmap& bitmap,
                     base::OnceCallback<void(SkBitmap)> callback) {
  DCHECK(callback);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Scale down bitmap on another thread.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ScaleDownBitmapOnIOThread, icon_size, std::move(bitmap)),
      std::move(callback));
}
