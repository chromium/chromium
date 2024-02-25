// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_ASSET_FETCHER_H_
#define ASH_PICKER_PICKER_ASSET_FETCHER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

class GURL;

namespace gfx {
class ImageSkia;
}

namespace ash {

namespace image_util {
struct AnimationFrame;
}  // namespace image_util

// Helper for asynchronously fetching Picker assets.
class ASH_EXPORT PickerAssetFetcher {
 public:
  // TODO: b/316936723 - Pass the frames by reference to avoid a copy.
  using PickerGifFetchedCallback =
      base::OnceCallback<void(std::vector<image_util::AnimationFrame>)>;

  using PickerImageFetchedCallback =
      base::OnceCallback<void(const gfx::ImageSkia&)>;

  virtual ~PickerAssetFetcher() = default;

  // Fetches and decodes a gif from `url`. If successful, the decoded gif frames
  // will be returned via `callback`. Otherwise, `callback` is run with an empty
  // vector of frames.
  virtual void FetchGifFromUrl(const GURL& url,
                               PickerGifFetchedCallback callback) = 0;

  // Fetches and decodes a gif preview image from `url`. If successful, the
  // decoded gif preview image will be returned via `callback`. Otherwise,
  // `callback` is run with an empty ImageSkia.
  virtual void FetchGifPreviewImageFromUrl(
      const GURL& url,
      PickerImageFetchedCallback callback) = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_ASSET_FETCHER_H_
