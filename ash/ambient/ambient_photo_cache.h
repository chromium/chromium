// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_
#define ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Holds the return value for |AmbientPhotoCache::ReadFiles| to use with
// |task_runner_->PostTaskAndReplyWithResult|.
// Represented on disk by a file for each of |image|, |details|, and
// |related_image|.
struct ASH_EXPORT PhotoCacheEntry {
  PhotoCacheEntry();

  PhotoCacheEntry(std::unique_ptr<std::string> image,
                  std::unique_ptr<std::string> details,
                  std::unique_ptr<std::string> related_image);

  PhotoCacheEntry(const PhotoCacheEntry&) = delete;
  PhotoCacheEntry& operator=(const PhotoCacheEntry&) = delete;
  PhotoCacheEntry(PhotoCacheEntry&&);

  ~PhotoCacheEntry();

  void reset();

  std::unique_ptr<std::string> image;
  std::unique_ptr<std::string> details;
  std::unique_ptr<std::string> related_image;
};

// Interface for downloading and decoding photos for Ambient mode. Mocked for
// testing to isolate from network and file system.
// Each cache entry is written to disk as three files in the |root_path|, with
// filenames prefixed by |cache_index|.
class ASH_EXPORT AmbientPhotoCache {
 public:
  AmbientPhotoCache() = default;
  AmbientPhotoCache(const AmbientPhotoCache&) = delete;
  AmbientPhotoCache& operator=(const AmbientPhotoCache&) = delete;
  virtual ~AmbientPhotoCache() = default;

  static std::unique_ptr<AmbientPhotoCache> Create(base::FilePath root_path);

  virtual void DownloadPhoto(
      const std::string& url,
      base::OnceCallback<void(std::unique_ptr<std::string>)> callback) = 0;

  // Saves the photo at |url| to |cache_index| and calls |callback| with a
  // boolean that indicates success. Setting |is_related| will change the
  // filename to indicate that this is a paired photo.
  virtual void DownloadPhotoToFile(const std::string& url,
                                   int cache_index,
                                   bool is_related,
                                   base::OnceCallback<void(bool)> callback) = 0;

  virtual void DecodePhoto(
      std::unique_ptr<std::string> data,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) = 0;

  // Write files to disk at |cache_index| and call |callback| when complete.
  // |image| and |related_image| are encoded jpg images that must be decoded
  // with |DecodePhoto| to display. |details| is human readable text.
  virtual void WriteFiles(int cache_index,
                          const std::string* const image,
                          const std::string* const details,
                          const std::string* const related_image,
                          base::OnceClosure callback) = 0;

  // Read the files at |cache_index| and call |callback| with a struct
  // containing the contents of each file. If a particular file fails to be
  // read, it may be represented as nullptr or empty string.
  virtual void ReadFiles(
      int cache_index,
      base::OnceCallback<void(PhotoCacheEntry)> callback) = 0;

  // Erase all stored files from disk.
  virtual void Clear() = 0;
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_
