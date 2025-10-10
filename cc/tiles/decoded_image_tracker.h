// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_DECODED_IMAGE_TRACKER_H_
#define CC_TILES_DECODED_IMAGE_TRACKER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/cc_export.h"
#include "cc/paint/target_color_params.h"
#include "cc/tiles/image_controller.h"

namespace cc {

// This class is the main interface for the rest of the system to request
// decodes. It is responsible for keeping the decodes locked for a number of
// tree commits (specified as |kNumCommitsToLock| in the implementation file) or
// until a timer expires (with delay specified by |kTimeoutDurationMs|).
//
// Note that it is safe to replace ImageController's cache without doing
// anything special with this class, since it retains only ids to the decode
// requests. When defunct ids are then used to try and unlock the image, they
// are silently ignored.
class CC_EXPORT DecodedImageTracker {
 public:
  explicit DecodedImageTracker(
      ImageController* controller,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  DecodedImageTracker(const DecodedImageTracker&) = delete;
  ~DecodedImageTracker();

  DecodedImageTracker& operator=(const DecodedImageTracker&) = delete;

  // Request that the given image be decoded. This issues a callback upon
  // completion. The callback takes a bool indicating whether the decode was
  // successful or not.
  void QueueImageDecode(const DrawImage& image,
                        base::OnceCallback<void(bool)> callback,
                        bool speculative);

  // Unlock all locked images - used to respond to memory pressure or
  // application background.
  void UnlockAllImages();

  // Notifies the tracker that images have been used, allowing it to
  // unlock them.
  void OnImagesUsedInDraw(const std::vector<DrawImage>& draw_images);

  void SetSyncTreeFrameNumber(int frame_number);

  void SetTickClockForTesting(
      const base::TickClock* tick_clock,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Test only functions:
  size_t NumLockedImagesForTesting() const { return locked_images_.size(); }

 private:
  friend class DecodedImageTrackerTest;

  void ImageDecodeFinished(base::OnceCallback<void(bool)> callback,
                           PaintImage::Id image_id,
                           ImageController::ImageDecodeRequestId request_id,
                           ImageController::ImageDecodeResult result);
  void CheckForExpiredDecodes();
  void StartTimer(base::TimeDelta);

  raw_ptr<ImageController> image_controller_;

  // Helper class tracking a locked image decode. Automatically releases the
  // lock using the provided DecodedImageTracker* on destruction.
  class ImageLock {
   public:
    ImageLock(DecodedImageTracker* tracker,
              ImageController::ImageDecodeRequestId request_id,
              int expiration_frame,
              base::TimeTicks expiration_time);
    ImageLock(const ImageLock&) = delete;
    ~ImageLock();

    ImageLock& operator=(const ImageLock&) = delete;
    base::TimeTicks expiration_time() const { return expiration_time_; }
    int expiration_frame() const { return expiration_frame_; }

   private:
    const raw_ptr<DecodedImageTracker> tracker_;
    const ImageController::ImageDecodeRequestId request_id_;
    int expiration_frame_;
    const base::TimeTicks expiration_time_;
  };
  base::flat_map<PaintImage::Id, std::unique_ptr<ImageLock>> locked_images_;

  int sync_tree_frame_number_ = -1;
  // Defaults to base::TimeTicks::Now(), but overridable for testing.
  raw_ptr<const base::TickClock> tick_clock_;
  std::unique_ptr<base::RepeatingTimer> expiration_timer_;
  std::unique_ptr<base::RepeatingClosure> timer_closure_;

  base::WeakPtrFactory<DecodedImageTracker> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_TILES_DECODED_IMAGE_TRACKER_H_
