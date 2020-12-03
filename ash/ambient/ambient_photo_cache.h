// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_
#define ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Interface for downloading and decoding photos for Ambient mode. Mocked for
// testing to isolate from network.
class AmbientPhotoCache {
 public:
  AmbientPhotoCache() = default;
  AmbientPhotoCache(const AmbientPhotoCache&) = delete;
  AmbientPhotoCache& operator=(const AmbientPhotoCache&) = delete;
  virtual ~AmbientPhotoCache() = default;

  static std::unique_ptr<AmbientPhotoCache> Create();

  virtual void DownloadPhoto(
      const std::string& url,
      base::OnceCallback<void(std::unique_ptr<std::string>)> callback) = 0;

  // Saves the photo to |file_path| and calls |callback| when complete. If an
  // error occurs, will call |callback| with an empty path.
  virtual void DownloadPhotoToFile(
      const std::string& url,
      base::OnceCallback<void(base::FilePath)> callback,
      const base::FilePath& file_path) = 0;

  virtual void DecodePhoto(
      std::unique_ptr<std::string> data,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) = 0;
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_
