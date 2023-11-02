// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_GENERATOR_IMAGE_THUMBNAIL_REQUEST_H_
#define CHROME_BROWSER_THUMBNAIL_GENERATOR_IMAGE_THUMBNAIL_REQUEST_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/image_decoder/image_decoder.h"

namespace base {
class FilePath;
}

// Helper class to generate thumbnail for a given local image file with a given
// max size. Must be invoked on the browser thread.
class ImageThumbnailRequest : public ImageDecoder::ImageRequest {
 public:
  ImageThumbnailRequest(int icon_size,
                        base::OnceCallback<void(const SkBitmap&)> callback);

  ImageThumbnailRequest(const ImageThumbnailRequest&) = delete;
  ImageThumbnailRequest& operator=(const ImageThumbnailRequest&) = delete;

  ~ImageThumbnailRequest() override;

  // Kicks off an asynchronous process to retrieve the thumbnail for the file
  // located at |file_path| with a max size of |icon_size_| in each dimension.
  // Invokes the |callback_| method when finished.
  void Start(const base::FilePath& path);

 private:
  // ImageDecoder::ImageRequest implementation.
  void OnImageDecoded(const SkBitmap& decoded_image) override;

  void OnDecodeImageFailed() override;

  void OnLoadComplete(std::vector<uint8_t> data);

  void FinishRequest(SkBitmap thumbnail);

  const int icon_size_;
  base::OnceCallback<void(const SkBitmap&)> callback_;
  base::WeakPtrFactory<ImageThumbnailRequest> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_THUMBNAIL_GENERATOR_IMAGE_THUMBNAIL_REQUEST_H_
