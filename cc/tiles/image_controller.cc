// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/image_controller.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/completion_event.h"
#include "cc/tiles/tile_task_manager.h"

namespace cc {

ImageController::ImageDecodeRequestId
    ImageController::s_next_image_decode_queue_id_ = 1;

ImageController::ImageController(
    base::SequencedTaskRunner* origin_task_runner,
    scoped_refptr<base::SequencedTaskRunner> worker_task_runner)
    : worker_task_runner_(std::move(worker_task_runner)),
      origin_task_runner_(origin_task_runner) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

ImageController::~ImageController() {
  StopWorkerTasks();
  for (auto& request : orphaned_decode_requests_)
    std::move(request.callback).Run(request.id, ImageDecodeResult::FAILURE);
}

void ImageController::StopWorkerTasks() {
  // We can't have worker threads without a cache_ or a worker_task_runner_, so
  // terminate early.
  if (!cache_ || !worker_task_runner_)
    return;

  // Abort all tasks that are currently scheduled to run (we'll wait for them to
  // finish next).
  {
    base::AutoLock hold(lock_);
    abort_tasks_ = true;
  }

  // Post a task that will simply signal a completion event to ensure that we
  // "flush" any scheduled tasks (they will abort).
  CompletionEvent completion_event;
  worker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce([](CompletionEvent* event) { event->Signal(); },
                                base::Unretained(&completion_event)));
  completion_event.Wait();

  // Reset the abort flag so that new tasks can be scheduled.
  {
    base::AutoLock hold(lock_);
    abort_tasks_ = false;
  }

  // Now that we flushed everything, if there was a task running and it
  // finished, it would have posted a completion callback back to the compositor
  // thread. We don't want that, so invalidate the weak ptrs again. Note that
  // nothing can start running between wait and this invalidate, since it would
  // only run on the current (compositor) thread.
  weak_ptr_factory_.InvalidateWeakPtrs();
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();

  // Now, begin cleanup.

  // Unlock all of the locked images (note that this vector would only be
  // populated if we actually need to unref the image.
  for (auto& image_pair : requested_locked_images_)
    cache_->UnrefImage(image_pair.second);
  requested_locked_images_.clear();

  // Now, complete the tasks that already ran but haven't completed. These would
  // be posted in the run loop, but since we invalidated the weak ptrs, we need
  // to run everything manually.
  for (auto& request_to_complete : requests_needing_completion_) {
    ImageDecodeRequest& request = request_to_complete.second;

    // The task (if one exists) would have run already, we just need to make
    // sure it was completed. Multiple requests for the same image use the same
    // task so it could have already been completed.
    if (request.task && !request.task->HasCompleted()) {
      request.task->OnTaskCompleted();
      request.task->DidComplete();
    }

    if (request.need_unref)
      cache_->UnrefImage(request.draw_image);

    // Orphan the request so that we can still run it when a new cache is set.
    request.task = nullptr;
    request.need_unref = false;
    orphaned_decode_requests_.push_back(std::move(request));
  }
  requests_needing_completion_.clear();

  // Finally, complete all of the tasks that never started running. This is
  // similar to the |requests_needing_completion_|, but happens at a different
  // stage in the pipeline.
  for (auto& request_pair : image_decode_queue_) {
    ImageDecodeRequest& request = request_pair.second;

    if (request.task) {
      // This task may have run via a different request, so only cancel it if
      // it's "new". That is, the same task could have been referenced by
      // several different image deque requests for the same image.
      if (request.task->state().IsNew())
        request.task->state().DidCancel();

      if (!request.task->HasCompleted()) {
        request.task->OnTaskCompleted();
        request.task->DidComplete();
      }
    }

    if (request.need_unref)
      cache_->UnrefImage(request.draw_image);

    // Orphan the request so that we can still run it when a new cache is set.
    request.task = nullptr;
    request.need_unref = false;
    orphaned_decode_requests_.push_back(std::move(request));
  }
  image_decode_queue_.clear();
}

void ImageController::SetImageDecodeCache(ImageDecodeCache* cache) {
  DCHECK(!cache_ || !cache);

  if (!cache) {
    SetPredecodeImages(std::vector<DrawImage>(),
                       ImageDecodeCache::TracingInfo());
    StopWorkerTasks();
    image_cache_max_limit_bytes_ = 0u;
  }

  cache_ = cache;

  if (cache_) {
    image_cache_max_limit_bytes_ = cache_->GetMaximumMemoryLimitBytes();
    GenerateTasksForOrphanedRequests();
  }
}

void ImageController::ConvertImagesToTasks(
    std::vector<DrawImage>* sync_decoded_images,
    std::vector<scoped_refptr<TileTask>>* tasks,
    bool* has_at_raster_images,
    const ImageDecodeCache::TracingInfo& tracing_info) {
  DCHECK(cache_);
  *has_at_raster_images = false;
  for (auto it = sync_decoded_images->begin();
       it != sync_decoded_images->end();) {
    // PaintWorklet images should not be included in this set; they have already
    // been painted before raster and so do not need raster-time work.
    DCHECK(!it->paint_image().IsPaintWorklet());

    ImageDecodeCache::TaskResult result =
        cache_->GetTaskForImageAndRef(*it, tracing_info);
    *has_at_raster_images |= result.IsAtRaster();
    if (result.task)
      tasks->push_back(std::move(result.task));
    if (result.need_unref)
      ++it;
    else
      it = sync_decoded_images->erase(it);
  }
}

void ImageController::UnrefImages(const std::vector<DrawImage>& images) {
  for (auto& image : images)
    cache_->UnrefImage(image);
}

void ImageController::ReduceMemoryUsage() {
  DCHECK(cache_);
  cache_->ReduceCacheUsage();
}

std::vector<scoped_refptr<TileTask>> ImageController::SetPredecodeImages(
    std::vector<DrawImage> images,
    const ImageDecodeCache::TracingInfo& tracing_info) {
  std::vector<scoped_refptr<TileTask>> new_tasks;
  bool has_at_raster_images = false;
  ConvertImagesToTasks(&images, &new_tasks, &has_at_raster_images,
                       tracing_info);
  UnrefImages(predecode_locked_images_);
  predecode_locked_images_ = std::move(images);
  return new_tasks;
}

ImageController::ImageDecodeRequestId ImageController::QueueImageDecode(
    const DrawImage& draw_image,
    ImageDecodedCallback callback) {
  // We must not receive any image requests if we have no worker.
  CHECK(worker_task_runner_);

  // Generate the next id.
  ImageDecodeRequestId id = s_next_image_decode_queue_id_++;

  DCHECK(draw_image.paint_image());
  bool is_image_lazy = draw_image.paint_image().IsLazyGenerated();

  // Get the tasks for this decode.
  ImageDecodeCache::TaskResult result(false);
  if (is_image_lazy)
    result = cache_->GetOutOfRasterDecodeTaskForImageAndRef(draw_image);
  // If we don't need to unref this, we don't actually have a task.
  DCHECK(result.need_unref || !result.task);

  // Schedule the task and signal that there is more work.
  base::AutoLock hold(lock_);
  image_decode_queue_[id] =
      ImageDecodeRequest(id, draw_image, std::move(callback),
                         std::move(result.task), result.need_unref);

  // If this is the only image decode request, schedule a task to run.
  // Otherwise, the task will be scheduled in the previou task's completion.
  if (image_decode_queue_.size() == 1) {
    // Post a worker task.
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ImageController::ProcessNextImageDecodeOnWorkerThread,
                       base::Unretained(this)));
  }

  return id;
}

void ImageController::UnlockImageDecode(ImageDecodeRequestId id) {
  // If the image exists, ie we actually need to unlock it, then do so.
  auto it = requested_locked_images_.find(id);
  if (it == requested_locked_images_.end())
    return;

  UnrefImages({std::move(it->second)});
  requested_locked_images_.erase(it);
}

void ImageController::ProcessNextImageDecodeOnWorkerThread() {
  TRACE_EVENT0("cc", "ImageController::ProcessNextImageDecodeOnWorkerThread");
  scoped_refptr<TileTask> decode_task;
  ImageDecodeRequestId decode_id;
  {
    base::AutoLock hold(lock_);

    // If we don't have any work, abort.
    if (image_decode_queue_.empty() || abort_tasks_)
      return;

    // Take the next request from the queue.
    auto decode_it = image_decode_queue_.begin();
    DCHECK(decode_it != image_decode_queue_.end());
    decode_task = decode_it->second.task;
    decode_id = decode_it->second.id;

    // Notify that the task will need completion. Note that there are two cases
    // where we process this. First, we might complete this task as a response
    // to the posted task below. Second, we might complete it in
    // StopWorkerTasks(). In either case, the task would have already run
    // (either post task happens after running, or the thread was already joined
    // which means the task ran). This means that we can put the decode into
    // |requests_needing_completion_| here before actually running the task.
    requests_needing_completion_[decode_id] = std::move(decode_it->second);

    image_decode_queue_.erase(decode_it);
  }

  // Run the task if we need to run it. If the task state isn't new, then
  // there is another task that is responsible for finishing it and cleaning
  // up (and it already ran); we just need to post a completion callback.
  // Note that the other tasks's completion will also run first, since the
  // requests are ordered. So, when we process this task's completion, we
  // won't actually do anything with the task and simply issue the callback.
  if (decode_task && decode_task->state().IsNew()) {
    decode_task->state().DidSchedule();
    decode_task->state().DidStart();
    decode_task->RunOnWorkerThread();
    decode_task->state().DidFinish();
  }
  origin_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ImageController::ImageDecodeCompleted,
                                weak_ptr_, decode_id));
}

void ImageController::ImageDecodeCompleted(ImageDecodeRequestId id) {
  ImageDecodedCallback callback;
  ImageDecodeResult result = ImageDecodeResult::SUCCESS;
  {
    base::AutoLock hold(lock_);

    auto request_it = requests_needing_completion_.find(id);
    DCHECK(request_it != requests_needing_completion_.end());
    id = request_it->first;
    ImageDecodeRequest& request = request_it->second;

    // First, Determine the status of the decode. This has to happen here, since
    // we conditionally move from the draw image below.
    // Also note that if we don't need an unref for a lazy decoded images, it
    // implies that we never attempted the decode. Some of the reasons for this
    // would be that the image is of an empty size, or if the image doesn't fit
    // into memory. In all cases, this implies that the decode was a failure.
    if (!request.draw_image.paint_image().IsLazyGenerated())
      result = ImageDecodeResult::DECODE_NOT_REQUIRED;
    else if (!request.need_unref)
      result = ImageDecodeResult::FAILURE;
    else
      result = ImageDecodeResult::SUCCESS;

    // If we need to unref this decode, then we have to put it into the locked
    // images vector.
    if (request.need_unref)
      requested_locked_images_[id] = std::move(request.draw_image);

    // If we have a task that isn't completed yet, we need to complete it.
    if (request.task && !request.task->HasCompleted()) {
      request.task->OnTaskCompleted();
      request.task->DidComplete();
    }

    // Finally, save the callback so we can run it without the lock, and erase
    // the request from |requests_needing_completion_|.
    callback = std::move(request.callback);
    requests_needing_completion_.erase(request_it);
  }

  // Post another task to run.
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageController::ProcessNextImageDecodeOnWorkerThread,
                     base::Unretained(this)));

  // Finally run the requested callback.
  std::move(callback).Run(id, result);
}

void ImageController::GenerateTasksForOrphanedRequests() {
  base::AutoLock hold(lock_);
  DCHECK_EQ(0u, image_decode_queue_.size());
  DCHECK_EQ(0u, requests_needing_completion_.size());
  DCHECK(cache_);

  for (auto& request : orphaned_decode_requests_) {
    DCHECK(!request.task);
    DCHECK(!request.need_unref);
    if (request.draw_image.paint_image().IsLazyGenerated()) {
      // Get the task for this decode.
      ImageDecodeCache::TaskResult result =
          cache_->GetOutOfRasterDecodeTaskForImageAndRef(request.draw_image);
      request.need_unref = result.need_unref;
      request.task = result.task;
    }
    image_decode_queue_[request.id] = std::move(request);
  }

  orphaned_decode_requests_.clear();
  if (!image_decode_queue_.empty()) {
    // Post a worker task.
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ImageController::ProcessNextImageDecodeOnWorkerThread,
                       base::Unretained(this)));
  }
}

ImageController::ImageDecodeRequest::ImageDecodeRequest() = default;
ImageController::ImageDecodeRequest::ImageDecodeRequest(
    ImageDecodeRequestId id,
    const DrawImage& draw_image,
    ImageDecodedCallback callback,
    scoped_refptr<TileTask> task,
    bool need_unref)
    : id(id),
      draw_image(draw_image),
      callback(std::move(callback)),
      task(std::move(task)),
      need_unref(need_unref) {}
ImageController::ImageDecodeRequest::ImageDecodeRequest(
    ImageDecodeRequest&& other) = default;
ImageController::ImageDecodeRequest::~ImageDecodeRequest() = default;

ImageController::ImageDecodeRequest& ImageController::ImageDecodeRequest::
operator=(ImageDecodeRequest&& other) = default;

}  // namespace cc
