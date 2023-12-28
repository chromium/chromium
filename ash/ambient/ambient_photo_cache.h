// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_
#define ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_

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

class AmbientAccessTokenController;

// Utilities for downloading and persisting photos for Ambient mode.
// Each cache entry is written to disk with filenames prefixed by |cache_index|.
namespace ambient_photo_cache {

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

// Sets the `task_runner` that will be used internally for all file
// operations. Must be called once before any other cache functions.
ASH_EXPORT void SetFileTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner);

ASH_EXPORT void DownloadPhoto(
    const std::string& url,
    AmbientAccessTokenController& access_token_controller,
    base::OnceCallback<void(std::string&&)> callback);

// Saves the photo at |url| to a temporary file and calls |callback| with a
// its path. If successful, the caller is responsible for deleting the
// temporary file when done using it. The returned path is empty on failure.
ASH_EXPORT void DownloadPhotoToTempFile(
    const std::string& url,
    AmbientAccessTokenController& access_token_controller,
    base::OnceCallback<void(base::FilePath)> callback);

// Write photo cache to disk at |cache_index| and call |callback| when
// complete.
ASH_EXPORT void WritePhotoCache(Store store,
                                int cache_index,
                                const ::ambient::PhotoCacheEntry& cache_entry,
                                base::OnceClosure callback);

// Read the photo cache at |cache_index| and call |callback| when complete.
// If a particular cache fails to be read, |cache_entry| will be empty.
ASH_EXPORT void ReadPhotoCache(
    Store store,
    int cache_index,
    base::OnceCallback<void(::ambient::PhotoCacheEntry cache_entry)> callback);

// Erase all stored files from disk.
ASH_EXPORT void Clear(Store store);

}  // namespace ambient_photo_cache
}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_
