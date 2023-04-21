// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FILE_PREVIEW_FILE_PREVIEW_IMAGE_SKIA_SOURCE_H_
#define ASH_PUBLIC_CPP_FILE_PREVIEW_FILE_PREVIEW_IMAGE_SKIA_SOURCE_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/files/file_path.h"
#include "base/timer/timer.h"
#include "ui/gfx/image/image_skia_source.h"

namespace ash {

class FilePreviewController;

namespace image_util {
struct AnimationFrame;
}  // namespace image_util

// The class responsible for the animation behavior that enables file previews.
// Invalidation going (effectively) directly from this class to the caller is
// what enables this behavior, circumventing the fact that `gfx::ImageSkia` and
// `ui::ImageModel` do not currently support invalidation.
// TODO(http://b/268372461): Add a `base::FilePathWatcher` (or other solution to
// watch for file changes) to this class.
class ASH_PUBLIC_EXPORT FilePreviewImageSkiaSource
    : public gfx::ImageSkiaSource {
 public:
  enum class PlaybackMode {
    // Does not animate, showing only the first frame.
    kFirstFrame,
    // Animates on a loop.
    kLoop,
  };

  FilePreviewImageSkiaSource(FilePreviewController* controller,
                             base::FilePath file_path);

  FilePreviewImageSkiaSource(const FilePreviewImageSkiaSource&) = delete;
  FilePreviewImageSkiaSource& operator=(const FilePreviewImageSkiaSource&) =
      delete;

  ~FilePreviewImageSkiaSource() override;

  // Changes the behavior of the animation through the frames. If there is a
  // single frame, this is effectively ignored. No-op if the new `PlaybackMode`
  // is the same as the current one.
  void SetPlaybackMode(PlaybackMode mode);

  // Invalidates all `base::WeakPtr`s and sets all raw pointers to `nullptr`.
  // This avoids doing extra work after an owning controller has been shutdown.
  void Shutdown();

  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

 private:
  // Decodes the frames for the file at `file_path_`.
  void FetchFrames();

  // Stores the fetched frames and triggers an update.
  void OnFramesFetched(std::vector<image_util::AnimationFrame> frames);

  // Advances to the next frame and triggers an update.
  void OnFrameTimer();

  // Starts or stops the frame timer as necessary based on `playback_mode`, and
  // triggers invalidation.
  void Update();

  base::raw_ptr<FilePreviewController> controller_;
  const base::FilePath file_path_;
  std::vector<image_util::AnimationFrame> frames_;
  size_t frame_index_ = 0;
  base::DeadlineTimer frame_timer_;
  bool fetching_frames_ = false;
  PlaybackMode playback_mode_ = PlaybackMode::kFirstFrame;

  base::WeakPtrFactory<FilePreviewImageSkiaSource> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FILE_PREVIEW_FILE_PREVIEW_IMAGE_SKIA_SOURCE_H_
