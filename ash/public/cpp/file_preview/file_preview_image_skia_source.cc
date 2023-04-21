// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/file_preview/file_preview_image_skia_source.h"

#include "ash/public/cpp/file_preview/file_preview_controller.h"
#include "ash/public/cpp/image_util.h"
#include "base/task/delay_policy.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash {

FilePreviewImageSkiaSource::FilePreviewImageSkiaSource(
    FilePreviewController* controller,
    base::FilePath file_path)
    : controller_(controller), file_path_(std::move(file_path)) {}

FilePreviewImageSkiaSource::~FilePreviewImageSkiaSource() = default;

void FilePreviewImageSkiaSource::SetPlaybackMode(PlaybackMode mode) {
  CHECK(controller_);
  if (playback_mode_ == mode) {
    return;
  }
  playback_mode_ = mode;

  Update();
}

void FilePreviewImageSkiaSource::Shutdown() {
  CHECK(controller_);
  // Once shutdown, do no more work and simply await destruction. No more
  // callbacks should execute.
  weak_factory_.InvalidateWeakPtrs();

  // Set the PlaybackMode to one that doesn't use `frame_timer_` to stop
  // animation and prevent it from being restarted if `OnFramesFetched()` is
  // called after `Shutdown()`.
  SetPlaybackMode(PlaybackMode::kFirstFrame);

  // The controller could be destroyed before `this`. To avoid use after free,
  // this method allows the controller to remove its raw pointer here.
  controller_ = nullptr;
}

void FilePreviewImageSkiaSource::FetchFrames() {
  CHECK(controller_);
  if (fetching_frames_) {
    return;
  }
  fetching_frames_ = true;
  image_util::DecodeAnimationFile(
      base::BindOnce(&FilePreviewImageSkiaSource::OnFramesFetched,
                     weak_factory_.GetWeakPtr()),
      file_path_);
}

void FilePreviewImageSkiaSource::OnFramesFetched(
    std::vector<image_util::AnimationFrame> frames) {
  CHECK(controller_);
  // TODO(http://b/277112811): Pass size down from caller and scale image here
  // once this has been wrapped in `ui::ImageModel::ImageGenerator`, since that
  // requires a known size before the file has been loaded.
  // Total size in memory of all frames is limited by the image decoder to be
  // less than `IPC::Channel::kMaximumMessageSize` (~134MB).
  frames_ = std::move(frames);
  Update();
  fetching_frames_ = false;
}

void FilePreviewImageSkiaSource::OnFrameTimer() {
  CHECK(controller_);
  CHECK(!frames_.empty());

  frame_index_ = (frame_index_ + 1) % frames_.size();
  Update();
}

void FilePreviewImageSkiaSource::Update() {
  CHECK(controller_);
  if (playback_mode_ == PlaybackMode::kFirstFrame) {
    frame_index_ = 0;
    frame_timer_.Stop();
  }

  // If the animation is looping and it is possible to start the frame timer,
  // and it isn't already running, then start it.
  if (!frame_timer_.IsRunning() && playback_mode_ == PlaybackMode::kLoop &&
      frames_.size() > 1) {
    // TODO(http://b/272078926): Possibly use another (more accurate) method of
    // timing frames.
    // TODO(http://b/272137556): Handle frames with 0 (or very small) duration.
    frame_timer_.Start(
        FROM_HERE, base::TimeTicks::Now() + frames_.at(frame_index_).duration,
        base::BindOnce(&FilePreviewImageSkiaSource::OnFrameTimer,
                       weak_factory_.GetWeakPtr()),
        base::subtle::DelayPolicy::kPrecise);
  }

  controller_->Invalidate(base::PassKey<FilePreviewImageSkiaSource>());
}

gfx::ImageSkiaRep FilePreviewImageSkiaSource::GetImageForScale(float scale) {
  if (frames_.empty()) {
    // TODO(http://b/266000882): Add logic to create static thumbnails for
    // non-gif files.
    FetchFrames();
    return gfx::ImageSkiaRep();
  }
  return frames_.at(frame_index_).image.GetRepresentation(scale);
}

}  // namespace ash
