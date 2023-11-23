// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_
#define ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace ash {

class AmbientClient;
class AmbientAccessTokenController;

// Interface for downloading and persisting photos for Ambient mode.
// Each cache entry is written to disk as three files in the |root_path|, with
// filenames prefixed by |cache_index|.
class ASH_EXPORT AmbientPhotoCache {
 public:
  // Each `Store` has a different directory (chosen internally) where ambient
  // photos are saved. Caller provides this below to specify which cache
  // directory to operate on.
  enum class Store {
    // Holds photos matching the most recent ambient topic source selected by
    // the user (ex: gphotos, art gallery, etc).
    kPrimary,
    // Holds a small fixed set of stock photos that do not match the ambient
    // topic source selected by the user. Only used if the primary store is
    // empty.
    kBackup
  };

  AmbientPhotoCache() = default;
  AmbientPhotoCache(const AmbientPhotoCache&) = delete;
  AmbientPhotoCache& operator=(const AmbientPhotoCache&) = delete;
  virtual ~AmbientPhotoCache() = default;

  // `root_path` is where the cached photos stored on disk.
  // `ambient_client` and `access_token_controller` are used to obtain url
  // loader factory and access tokens to fetch the online images.
  static std::unique_ptr<AmbientPhotoCache> Create(
      base::FilePath root_path,
      AmbientClient& ambient_client,
      AmbientAccessTokenController& access_token_controller);

  // Overrides the output of |Create()| for testing. This is global and can be
  // reset back to a null callback to disable the override.
  static void SetFactoryForTesting(
      base::RepeatingCallback<std::unique_ptr<AmbientPhotoCache>()> factory_fn);

  // Sets the `task_runner` that will be used internally for all file
  // operations. Must be called once before any other cache functions.
  static void SetFileTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  static void DownloadPhoto(
      const std::string& url,
      AmbientAccessTokenController& access_token_controller,
      base::OnceCallback<void(std::string&&)> callback);

  // Saves the photo at |url| to |cache_index| and calls |callback| with a
  // boolean that indicates success.
  static void DownloadPhotoToFile(
      Store store,
      const std::string& url,
      AmbientAccessTokenController& access_token_controller,
      int cache_index,
      base::OnceCallback<void(bool)> callback);

  // Write photo cache to disk at |cache_index| and call |callback| when
  // complete.
  static void WritePhotoCache(Store store,
                              int cache_index,
                              const ::ambient::PhotoCacheEntry& cache_entry,
                              base::OnceClosure callback);

  // Read the photo cache at |cache_index| and call |callback| when complete.
  // If a particular cache fails to be read, |cache_entry| will be empty.
  static void ReadPhotoCache(
      Store store,
      int cache_index,
      base::OnceCallback<void(::ambient::PhotoCacheEntry cache_entry)>
          callback);

  // Erase all stored files from disk.
  static void Clear(Store store);
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_
