// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/decoded_image_tracker.h"

#include <algorithm>

#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"

namespace cc {
namespace {
// Release decoded data lock after this number of commits, to prevent unbounded
// cache usage.
const int kNumCommitsToLock = 3;

// Timeout images after 4000ms, whether or not they've been used. This is a
// fallback for kNumCommitsToLock in the event that the widget is not producing
// commits (because it's throttled or paused).
const int64_t kTimeoutDurationMs = 4000;
}  // namespace

DecodedImageTracker::ImageLock::ImageLock(
    DecodedImageTracker* tracker,
    ImageController::ImageDecodeRequestId request_id,
    int expiration_frame,
    base::TimeTicks expiration_time)
    : tracker_(tracker),
      request_id_(request_id),
      expiration_frame_(expiration_frame),
      expiration_time_(expiration_time) {}

DecodedImageTracker::ImageLock::~ImageLock() {
  tracker_->image_controller_->UnlockImageDecode(request_id_);
}

DecodedImageTracker::DecodedImageTracker(
    ImageController* controller,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : image_controller_(controller),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      expiration_timer_(std::make_unique<base::RepeatingTimer>(tick_clock_)) {
  DCHECK(image_controller_);
  expiration_timer_->SetTaskRunner(task_runner);
  // This must be done after weak_ptr_factory_ is initialized.
  timer_closure_ = std::make_unique<base::RepeatingClosure>(
      base::BindRepeating(&DecodedImageTracker::CheckForExpiredDecodes,
                          weak_ptr_factory_.GetWeakPtr()));
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
                     image.paint_image().stable_id()),
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

void DecodedImageTracker::SetSyncTreeFrameNumber(int frame_number) {
  if (sync_tree_frame_number_ != frame_number) {
    sync_tree_frame_number_ = frame_number;
    CheckForExpiredDecodes();
  }
}

void DecodedImageTracker::CheckForExpiredDecodes() {
  if (locked_images_.empty()) {
    return;
  }

  int current_frame = sync_tree_frame_number_;
  auto now = tick_clock_->NowTicks();
  base::TimeDelta new_delay = base::TimeDelta::Max();
  base::EraseIf(
      locked_images_,
      [now, current_frame, &new_delay](
          const std::pair<PaintImage::Id, std::unique_ptr<ImageLock>>& entry)
          -> bool {
        if (entry.second->expiration_frame() < current_frame ||
            entry.second->expiration_time() <= now) {
          return true;
        }
        new_delay = std::min(new_delay, entry.second->expiration_time() - now);
        return false;
      });
  StartTimer(new_delay);
}

void DecodedImageTracker::SetTickClockForTesting(
    const base::TickClock* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  tick_clock_ = tick_clock;
  expiration_timer_ = std::make_unique<base::RepeatingTimer>(tick_clock_);
  expiration_timer_->SetTaskRunner(task_runner);
}

void DecodedImageTracker::ImageDecodeFinished(
    base::OnceCallback<void(bool)> callback,
    PaintImage::Id image_id,
    ImageController::ImageDecodeRequestId request_id,
    ImageController::ImageDecodeResult result) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "DecodedImageTracker::ImageDecodeFinished");

  if (result == ImageController::ImageDecodeResult::SUCCESS) {
    // If this image already exists, just replace it with the new (latest)
    // decode.
    locked_images_.erase(image_id);
    auto delay = base::Milliseconds(kTimeoutDurationMs);
    locked_images_.emplace(
        image_id,
        std::make_unique<ImageLock>(this, request_id,
                                    sync_tree_frame_number_ + kNumCommitsToLock,
                                    tick_clock_->NowTicks() + delay));
    if (!expiration_timer_->IsRunning()) {
      StartTimer(delay);
    }
  }
  bool decode_succeeded =
      result == ImageController::ImageDecodeResult::SUCCESS ||
      result == ImageController::ImageDecodeResult::DECODE_NOT_REQUIRED;
  std::move(callback).Run(decode_succeeded);
}

void DecodedImageTracker::StartTimer(base::TimeDelta delay) {
  expiration_timer_->Stop();
  if (delay == base::TimeDelta::Max()) {
    return;
  }
  expiration_timer_->Start(FROM_HERE, delay, *timer_closure_);
}

}  // namespace cc
