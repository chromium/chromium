// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_DECODED_IMAGE_TRACKER_H_
#define CC_TILES_DECODED_IMAGE_TRACKER_H_

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/tiles/image_controller.h"

namespace cc {

// This class is the main interface for the rest of the system to request
// decodes. It is responsible for keeping the decodes locked for a number of
// frames, specified as |kNumFramesToLock| in the implementation file.
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
  void QueueImageDecode(const PaintImage& image,
                        const gfx::ColorSpace& target_color_space,
                        base::OnceCallback<void(bool)> callback);

  // Unlock all locked images - used to respond to memory pressure or
  // application background.
  void UnlockAllImages();

  // Notifies the tracker that images have been used, allowing it to
  // unlock them.
  void OnImagesUsedInDraw(const std::vector<DrawImage>& draw_images);

  void SetTickClockForTesting(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

  // Test only functions:
  size_t NumLockedImagesForTesting() const { return locked_images_.size(); }

 private:
  friend class DecodedImageTrackerTest;

  void ImageDecodeFinished(base::OnceCallback<void(bool)> callback,
                           PaintImage::Id image_id,
                           ImageController::ImageDecodeRequestId request_id,
                           ImageController::ImageDecodeResult result);
  void OnTimeoutImages();
  void EnqueueTimeout();

  ImageController* image_controller_;

  // Helper class tracking a locked image decode. Automatically releases the
  // lock using the provided DecodedImageTracker* on destruction.
  class ImageLock {
   public:
    ImageLock(DecodedImageTracker* tracker,
              ImageController::ImageDecodeRequestId request_id,
              base::TimeTicks lock_time);
    ImageLock(const ImageLock&) = delete;
    ~ImageLock();

    ImageLock& operator=(const ImageLock&) = delete;
    base::TimeTicks lock_time() const { return lock_time_; }

   private:
    DecodedImageTracker* tracker_;
    ImageController::ImageDecodeRequestId request_id_;
    base::TimeTicks lock_time_;
  };
  base::flat_map<PaintImage::Id, std::unique_ptr<ImageLock>> locked_images_;
  bool timeout_pending_ = false;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Defaults to base::TimeTicks::Now(), but overrideable for testing.
  const base::TickClock* tick_clock_;

  base::WeakPtrFactory<DecodedImageTracker> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_TILES_DECODED_IMAGE_TRACKER_H_
