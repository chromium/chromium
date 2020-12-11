// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_CONTROLLER_H_
#define CC_TILES_IMAGE_CONTROLLER_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/simple_thread.h"
#include "cc/base/unique_notifier.h"
#include "cc/cc_export.h"
#include "cc/paint/draw_image.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/image_decode_cache.h"

namespace cc {

class CC_EXPORT ImageController {
 public:
  enum class ImageDecodeResult { SUCCESS, DECODE_NOT_REQUIRED, FAILURE };

  using ImageDecodeRequestId = uint64_t;
  using ImageDecodedCallback =
      base::OnceCallback<void(ImageDecodeRequestId, ImageDecodeResult)>;
  explicit ImageController(
      base::SequencedTaskRunner* origin_task_runner,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner);
  ImageController(const ImageController&) = delete;
  virtual ~ImageController();

  ImageController& operator=(const ImageController&) = delete;

  void SetImageDecodeCache(ImageDecodeCache* cache);
  // Build tile tasks for synchronously decoded images.
  // |sync_decoded_images| is the input. These are the images from a particular
  // tile, retrieved by the DiscardableImageMap. Images can be removed from the
  // vector under certain conditions.
  // |tasks| is an output, which are the built tile tasks.
  // |has_at_raster_images| is an output parameter.
  // |has_hardware_accelerated_jpeg_candidates| and
  // |has_hardware_accelerated_webp_candidates| are output parameters that
  // indicate if there are images in |sync_decoded_images| that could be decoded
  // using hardware decode acceleration.
  // |tracing_info| is used in tracing or UMA only.
  void ConvertImagesToTasks(std::vector<DrawImage>* sync_decoded_images,
                            std::vector<scoped_refptr<TileTask>>* tasks,
                            bool* has_at_raster_images,
                            bool* has_hardware_accelerated_jpeg_candidates,
                            bool* has_hardware_accelerated_webp_candidates,
                            const ImageDecodeCache::TracingInfo& tracing_info);
  void UnrefImages(const std::vector<DrawImage>& images);
  void ReduceMemoryUsage();
  std::vector<scoped_refptr<TileTask>> SetPredecodeImages(
      std::vector<DrawImage> predecode_images,
      const ImageDecodeCache::TracingInfo& tracing_info);

  // Virtual for testing.
  virtual void UnlockImageDecode(ImageDecodeRequestId id);

  // This function requests that the given image be decoded and locked. Once the
  // callback has been issued, it is passed an ID, which should be used to
  // unlock this image. It is up to the caller to ensure that the image is later
  // unlocked using UnlockImageDecode.
  // Virtual for testing.
  virtual ImageDecodeRequestId QueueImageDecode(const DrawImage& draw_image,
                                                ImageDecodedCallback callback);
  size_t image_cache_max_limit_bytes() const {
    return image_cache_max_limit_bytes_;
  }

  void SetMaxImageCacheLimitBytesForTesting(size_t bytes) {
    image_cache_max_limit_bytes_ = bytes;
  }

  ImageDecodeCache* cache() const { return cache_; }

 protected:
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

 private:
  struct ImageDecodeRequest {
    ImageDecodeRequest();
    ImageDecodeRequest(ImageDecodeRequestId id,
                       const DrawImage& draw_image,
                       ImageDecodedCallback callback,
                       scoped_refptr<TileTask> task,
                       bool need_unref);
    ImageDecodeRequest(ImageDecodeRequest&& other);
    ~ImageDecodeRequest();

    ImageDecodeRequest& operator=(ImageDecodeRequest&& other);

    ImageDecodeRequestId id;
    DrawImage draw_image;
    ImageDecodedCallback callback;
    scoped_refptr<TileTask> task;
    bool need_unref;
  };

  void StopWorkerTasks();

  // Called from the worker thread.
  void ProcessNextImageDecodeOnWorkerThread();

  void ImageDecodeCompleted(ImageDecodeRequestId id);
  void GenerateTasksForOrphanedRequests();

  base::WeakPtr<ImageController> weak_ptr_;

  ImageDecodeCache* cache_ = nullptr;
  std::vector<DrawImage> predecode_locked_images_;

  static ImageDecodeRequestId s_next_image_decode_queue_id_;
  base::flat_map<ImageDecodeRequestId, DrawImage> requested_locked_images_;

  base::SequencedTaskRunner* origin_task_runner_ = nullptr;
  size_t image_cache_max_limit_bytes_ = 0u;

  // The variables defined below this lock (aside from weak_ptr_factory_) can
  // only be accessed when the lock is acquired.
  base::Lock lock_;
  std::map<ImageDecodeRequestId, ImageDecodeRequest> image_decode_queue_;
  std::map<ImageDecodeRequestId, ImageDecodeRequest>
      requests_needing_completion_;
  bool abort_tasks_ = false;
  // Orphaned requests are requests that were either in queue or needed a
  // completion callback when we set the decode cache to be nullptr. When a new
  // decode cache is set, these requests are re-enqueued again with tasks
  // generated by the new cache. Note that when the cache is set, then aside
  // from generating new tasks, this vector should be empty.
  std::vector<ImageDecodeRequest> orphaned_decode_requests_;

  base::WeakPtrFactory<ImageController> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_TILES_IMAGE_CONTROLLER_H_
