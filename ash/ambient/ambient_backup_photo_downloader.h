// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_BACKUP_PHOTO_DOWNLOADER_H_
#define ASH_AMBIENT_AMBIENT_BACKUP_PHOTO_DOWNLOADER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class AmbientAccessTokenController;

// Downloads a "backup" image from a fixed url, resizes it to roughly match a
// target size provided by the caller, and saves it to disc. After it's
// downloaded, the backup image may be used in future ambient sessions if there
// are issues fetching the primary stream of photos from the backend.
//
// `AmbientBackupPhotoDownloader` only downloads/saves one photo in its lifetime
// and runs the `completion_cb` when done. It may be destroyed at any point to
// stop whatever task it's running internally, and the `completion_cb` will not
// be run.
class ASH_EXPORT AmbientBackupPhotoDownloader {
 public:
  AmbientBackupPhotoDownloader(
      AmbientAccessTokenController& access_token_controller,
      int cache_idx,
      gfx::Size target_size,
      const std::string& url,
      base::OnceCallback<void(bool success)> completion_cb);
  AmbientBackupPhotoDownloader(const AmbientBackupPhotoDownloader&) = delete;
  AmbientBackupPhotoDownloader& operator=(const AmbientBackupPhotoDownloader&) =
      delete;
  ~AmbientBackupPhotoDownloader();

 private:
  void RunCompletionCallback(bool success);
  void DecodeImage(base::FilePath temp_image_path);
  void ScheduleResizeAndEncode(const gfx::ImageSkia& decoded_image);
  void SaveImage(const std::vector<unsigned char>& encoded_image);

  const int cache_idx_;
  const gfx::Size target_size_;
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::OnceCallback<void(bool success)> completion_cb_;
  // Temporary location where the image is downloaded first before it is
  // resized and then saved to the final destination.
  base::FilePath temp_image_path_;
  base::WeakPtrFactory<AmbientBackupPhotoDownloader> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_BACKUP_PHOTO_DOWNLOADER_H_
