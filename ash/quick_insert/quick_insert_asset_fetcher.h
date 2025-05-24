// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_QUICK_INSERT_ASSET_FETCHER_H_
#define ASH_QUICK_INSERT_QUICK_INSERT_ASSET_FETCHER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/files/file.h"
#include "base/functional/callback_forward.h"

class GURL;
class SkBitmap;

namespace gfx {
class ImageSkia;
class Size;
}  // namespace gfx

namespace network {
class SimpleURLLoader;
}

namespace ash {

namespace image_util {
struct AnimationFrame;
}  // namespace image_util

// Helper for asynchronously fetching Quick Insert assets.
class ASH_EXPORT QuickInsertAssetFetcher {
 public:
  // TODO: b/316936723 - Pass the frames by reference to avoid a copy.
  using QuickInsertGifFetchedCallback =
      base::OnceCallback<void(std::vector<image_util::AnimationFrame>)>;
  using QuickInsertImageFetchedCallback =
      base::OnceCallback<void(const gfx::ImageSkia&)>;
  using FetchFileThumbnailCallback =
      base::OnceCallback<void(const SkBitmap* bitmap, base::File::Error error)>;

  virtual ~QuickInsertAssetFetcher() = default;

  // Fetches and decodes a gif from `url`. If successful, the decoded gif frames
  // will be returned via `callback`. Otherwise, `callback` is run with an empty
  // vector of frames.
  // `rank` is the index of the GIF in the results. The lower the rank, the
  // higher its priority.
  // If the returned `SimpleURLLoader` is destroyed before the request is
  // complete, the request is canceled and `callback` will not be called.
  [[nodiscard]] virtual std::unique_ptr<network::SimpleURLLoader>
  FetchGifFromUrl(const GURL& url,
                  size_t rank,
                  QuickInsertGifFetchedCallback callback) = 0;

  // Fetches and decodes a gif preview image from `url`. If successful, the
  // decoded gif preview image will be returned via `callback`. Otherwise,
  // `callback` is run with an empty ImageSkia.
  // `rank` is the index of the GIF in the results. The lower the rank, the
  // higher its priority.
  // If the returned `SimpleURLLoader` is destroyed before the request is
  // complete, the request is canceled and `callback` will not be called.
  [[nodiscard]] virtual std::unique_ptr<network::SimpleURLLoader>
  FetchGifPreviewImageFromUrl(const GURL& url,
                              size_t rank,
                              QuickInsertImageFetchedCallback callback) = 0;

  // Fetches the thumbnail for a file and calls `callback` with the result.
  virtual void FetchFileThumbnail(const base::FilePath& path,
                                  const gfx::Size& size,
                                  FetchFileThumbnailCallback callback) = 0;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_ASSET_FETCHER_H_
