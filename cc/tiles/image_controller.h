// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_CONTROLLER_H_
#define CC_TILES_IMAGE_CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
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
      scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
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

  enum class WorkerTaskState {
    kNoTask,
    kQueuedTask,
    kRunningTask,
  };

  // State accessible from the worker thread. Held in a isolated struct so it
  // can be deleted asynchronously on the worker thread after the
  // ImageController is deleted.
  struct WorkerState {
    WorkerState(scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
                base::WeakPtr<ImageController> weak_ptr);
    ~WorkerState();

    base::Lock lock;
    std::map<ImageDecodeRequestId, ImageDecodeRequest> image_decode_queue
        GUARDED_BY(lock);
    std::map<ImageDecodeRequestId, ImageDecodeRequest>
        requests_needing_completion GUARDED_BY(lock);
    WorkerTaskState task_state GUARDED_BY(lock) = WorkerTaskState::kNoTask;

    const scoped_refptr<base::SequencedTaskRunner> origin_task_runner;
    const base::WeakPtr<ImageController> weak_ptr;
  };

  void StopWorkerTasks();

  static void ProcessNextImageDecodeOnWorkerThread(WorkerState* worker_state);

  void ImageDecodeCompleted(ImageDecodeRequestId id);
  void GenerateTasksForOrphanedRequests();

  void ScheduleImageDecodeOnWorkerIfNeeded()
      EXCLUSIVE_LOCKS_REQUIRED(worker_state_->lock);

  // RAW_PTR_EXCLUSION: ImageDecodeCache is marked as not supported by raw_ptr.
  // See raw_ptr.h for more information.
  RAW_PTR_EXCLUSION ImageDecodeCache* cache_ = nullptr;
  std::vector<DrawImage> predecode_locked_images_;

  static ImageDecodeRequestId s_next_image_decode_queue_id_;
  base::flat_map<ImageDecodeRequestId, DrawImage> requested_locked_images_;
  size_t image_cache_max_limit_bytes_ = 0u;

  std::unique_ptr<WorkerState> worker_state_;

  // Orphaned requests are requests that were either in queue or needed a
  // completion callback when we set the decode cache to be nullptr. When a new
  // decode cache is set, these requests are re-enqueued again with tasks
  // generated by the new cache. Note that when the cache is set, then aside
  // from generating new tasks, this vector should be empty.
  std::vector<ImageDecodeRequest> orphaned_decode_requests_;

  // The id generated by ImageDecodeCache instance to identify this client
  // instance when requesting image tasks.
  ImageDecodeCache::ClientId image_cache_client_id_ = 0;

  base::WeakPtrFactory<ImageController> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_TILES_IMAGE_CONTROLLER_H_
