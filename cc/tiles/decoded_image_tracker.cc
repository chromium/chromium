// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/decoded_image_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"

namespace cc {
namespace {
// Timeout images after 250ms, whether or not they've been used. This prevents
// unbounded cache usage.
const int64_t kTimeoutDurationMs = 250;

// Timeout for speculative image decodes. This is longer than the regular
// timeout because during load, when there tends to be a lot of slow tasks
// and the rendering workload is heavy, it's easy for these decodes to expire
// before the LCP frame reaches commit.
// TODO: It probably makes more sense for the expiration to expressed as a
// number of subsequent commits (2-3 probably) rather than a timer.
const int64_t kSpeculativeDecodeTimeoutDurationMs = 1000;
}  // namespace

DecodedImageTracker::ImageLock::ImageLock(
    DecodedImageTracker* tracker,
    ImageController::ImageDecodeRequestId request_id,
    base::TimeTicks expiration)
    : tracker_(tracker), request_id_(request_id), expiration_(expiration) {}

DecodedImageTracker::ImageLock::~ImageLock() {
  tracker_->image_controller_->UnlockImageDecode(request_id_);
}

DecodedImageTracker::DecodedImageTracker(
    ImageController* controller,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : image_controller_(controller),
      task_runner_(std::move(task_runner)),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  DCHECK(image_controller_);
}

DecodedImageTracker::~DecodedImageTracker() {
  UnlockAllImages();
}

void DecodedImageTracker::QueueImageDecode(
    const DrawImage& image,
    base::OnceCallback<void(bool)> callback,
    bool speculative) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "DecodedImageTracker::QueueImageDecode", "frame_key",
               image.frame_key().ToString());
  DCHECK(image_controller_);
  // Queue the decode in the image controller, but switch out the callback for
  // our own.
  image_controller_->QueueImageDecode(
      image,
      base::BindOnce(&DecodedImageTracker::ImageDecodeFinished,
                     base::Unretained(this), std::move(callback),
                     image.paint_image().stable_id(), speculative),
      speculative);
}

void DecodedImageTracker::UnlockAllImages() {
  locked_images_.clear();
}

void DecodedImageTracker::OnImagesUsedInDraw(
    const std::vector<DrawImage>& draw_images) {
  for (const DrawImage& draw_image : draw_images)
    locked_images_.erase(draw_image.paint_image().stable_id());
}

void DecodedImageTracker::ImageDecodeFinished(
    base::OnceCallback<void(bool)> callback,
    PaintImage::Id image_id,
    bool speculative,
    ImageController::ImageDecodeRequestId request_id,
    ImageController::ImageDecodeResult result) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "DecodedImageTracker::ImageDecodeFinished");

  if (result == ImageController::ImageDecodeResult::SUCCESS) {
    // If this image already exists, just replace it with the new (latest)
    // decode.
    locked_images_.erase(image_id);
    auto timeout = base::Milliseconds(
        speculative ? kSpeculativeDecodeTimeoutDurationMs : kTimeoutDurationMs);
    locked_images_.emplace(
        image_id, std::make_unique<ImageLock>(
                      this, request_id, tick_clock_->NowTicks() + timeout));
    EnqueueTimeout();
  }
  bool decode_succeeded =
      result == ImageController::ImageDecodeResult::SUCCESS ||
      result == ImageController::ImageDecodeResult::DECODE_NOT_REQUIRED;
  std::move(callback).Run(decode_succeeded);
}

void DecodedImageTracker::OnTimeoutImages() {
  timeout_pending_ = false;
  if (locked_images_.size() == 0)
    return;

  auto now = tick_clock_->NowTicks();
  for (auto it = locked_images_.begin(); it != locked_images_.end();) {
    auto& image = it->second;
    if (now < image->expiration()) {
      ++it;
      continue;
    }
    it = locked_images_.erase(it);
  }

  EnqueueTimeout();
}

void DecodedImageTracker::EnqueueTimeout() {
  if (timeout_pending_)
    return;
  if (locked_images_.size() == 0)
    return;

  timeout_pending_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DecodedImageTracker::OnTimeoutImages,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(kTimeoutDurationMs));
}

}  // namespace cc
