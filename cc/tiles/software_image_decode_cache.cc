// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/software_image_decode_cache.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/debug/stack_trace.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/ostream_operators.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/features.h"
#include "cc/base/histograms.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/mipmap_util.h"
#include "components/miracle_parameter/common/public/miracle_parameter.h"
#include "ui/gfx/geometry/skia_conversions.h"

using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpLevelOfDetail;

namespace cc {
namespace {

BASE_FEATURE(kNormalMaxItemsInCacheForSoftwareFeature,
             "NormalMaxItemsInCacheForSoftwareFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The number of entries to keep around in the cache. This limit can be breached
// if more items are locked. That is, locked items ignore this limit.
// Depending on the memory state of the system, we limit the amount of items
// differently.
MIRACLE_PARAMETER_FOR_INT(GetNormalMaxItemsInCacheForSoftware,
                          kNormalMaxItemsInCacheForSoftwareFeature,
                          "NormalMaxItemsInCacheForSoftware",
                          1000)

class AutoRemoveKeyFromTaskMap {
 public:
  AutoRemoveKeyFromTaskMap(
      std::unordered_map<SoftwareImageDecodeCache::CacheKey,
                         scoped_refptr<TileTask>,
                         SoftwareImageDecodeCache::CacheKeyHash>* task_map,
      const SoftwareImageDecodeCache::CacheKey& key)
      : task_map_(task_map), key_(key) {}
  ~AutoRemoveKeyFromTaskMap() { task_map_->erase(*key_); }

 private:
  raw_ptr<std::unordered_map<SoftwareImageDecodeCache::CacheKey,
                             scoped_refptr<TileTask>,
                             SoftwareImageDecodeCache::CacheKeyHash>>
      task_map_;
  const raw_ref<const SoftwareImageDecodeCache::CacheKey> key_;
};

class SoftwareImageDecodeTaskImpl : public TileTask {
 public:
  SoftwareImageDecodeTaskImpl(
      SoftwareImageDecodeCache* cache,
      const SoftwareImageDecodeCache::CacheKey& image_key,
      const PaintImage& paint_image,
      ImageDecodeCache::TaskType task_type,
      const ImageDecodeCache::TracingInfo& tracing_info)
      : TileTask(TileTask::SupportsConcurrentExecution::kYes,
                 TileTask::SupportsBackgroundThreadPriority::kNo),
        cache_(cache),
        image_key_(image_key),
        paint_image_(paint_image),
        task_type_(task_type),
        tracing_info_(tracing_info) {}
  SoftwareImageDecodeTaskImpl(const SoftwareImageDecodeTaskImpl&) = delete;

  SoftwareImageDecodeTaskImpl& operator=(const SoftwareImageDecodeTaskImpl&) =
      delete;

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "SoftwareImageDecodeTaskImpl::RunOnWorkerThread", "mode",
                 "software", "source_prepare_tiles_id",
                 tracing_info_.prepare_tiles_id);

    const auto* image_metadata = paint_image_.GetImageHeaderMetadata();
    const ImageType image_type =
        image_metadata ? image_metadata->image_type : ImageType::kInvalid;
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        paint_image_.GetSwSkImage().get(),
        devtools_instrumentation::ScopedImageDecodeTask::DecodeType::kSoftware,
        ImageDecodeCache::ToScopedTaskType(task_type_),
        ImageDecodeCache::ToScopedImageType(image_type));
    SoftwareImageDecodeCache::TaskProcessingResult result =
        cache_->DecodeImageInTask(image_key_, paint_image_, task_type_);

    // Do not log timing UMAs if we did not perform a full decode.
    if (result != SoftwareImageDecodeCache::TaskProcessingResult::kFullDecode)
      image_decode_task.SuppressMetrics();
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    cache_->OnImageDecodeTaskCompleted(image_key_, task_type_);
  }

  // Overridden from TileTask:
  bool TaskContainsLCPCandidateImages() const override {
    if (!HasCompleted() && paint_image_.may_be_lcp_candidate())
      return true;
    return TileTask::TaskContainsLCPCandidateImages();
  }

 protected:
  ~SoftwareImageDecodeTaskImpl() override = default;

 private:
  raw_ptr<SoftwareImageDecodeCache, AcrossTasksDanglingUntriaged> cache_;
  SoftwareImageDecodeCache::CacheKey image_key_;
  PaintImage paint_image_;
  ImageDecodeCache::TaskType task_type_;
  const ImageDecodeCache::TracingInfo tracing_info_;
};

SkSize GetScaleAdjustment(const SoftwareImageDecodeCache::CacheKey& key) {
  // If the requested filter quality did not require scale, then the adjustment
  // is identity.
  if (key.type() != SoftwareImageDecodeCache::CacheKey::kSubrectAndScale) {
    return SkSize::Make(1.f, 1.f);
  } else {
    return MipMapUtil::GetScaleAdjustmentForSize(key.src_rect().size(),
                                                 key.target_size());
  }
}

// Returns the filter quality to be used with the decoded result of the image.
// Note that in most cases this yields Low filter quality, meaning bilinear
// interpolation. This is because the processing for the image would have
// already been done, including scaling down to a mip level. So what remains is
// to do a bilinear interpolation. The exception to this is if the developer
// specified a pixelated effect, which results in a None filter quality (nearest
// neighbor).
PaintFlags::FilterQuality GetDecodedFilterQuality(
    const SoftwareImageDecodeCache::CacheKey& key) {
  return key.is_nearest_neighbor() ? PaintFlags::FilterQuality::kNone
                                   : PaintFlags::FilterQuality::kLow;
}

}  // namespace

SoftwareImageDecodeCache::SoftwareImageDecodeCache(
    SkColorType color_type,
    size_t locked_memory_limit_bytes)
    : decoded_images_(ImageLRUCache::NO_AUTO_EVICT),
      locked_images_budget_(locked_memory_limit_bytes),
      color_type_(color_type),
      generator_client_id_(PaintImage::GetNextGeneratorClientId()),
      max_items_in_cache_(GetNormalMaxItemsInCacheForSoftware()) {
  DCHECK_NE(generator_client_id_, PaintImage::kDefaultGeneratorClientId);
  // In certain cases, SingleThreadTaskRunner::CurrentDefaultHandle isn't set
  // (Android Webview).  Don't register a dump provider in these cases.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::SoftwareImageDecodeCache",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

SoftwareImageDecodeCache::~SoftwareImageDecodeCache() {
  // It is safe to unregister, even if we didn't register in the constructor.
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

ImageDecodeCache::TaskResult SoftwareImageDecodeCache::GetTaskForImageAndRef(
    ClientId client_id,
    const DrawImage& image,
    const TracingInfo& tracing_info) {
  DCHECK_EQ(client_id, ImageDecodeCache::kDefaultClientId)
      << "SoftwareImageDecodeCache cannot be shared between multiple clients.";
  return GetTaskForImageAndRefInternal(image, tracing_info,
                                       TaskType::kInRaster);
}

ImageDecodeCache::TaskResult
SoftwareImageDecodeCache::GetOutOfRasterDecodeTaskForImageAndRef(
    ClientId client_id,
    const DrawImage& image) {
  DCHECK_EQ(client_id, ImageDecodeCache::kDefaultClientId)
      << "SoftwareImageDecodeCache cannot be shared between multiple clients.";
  return GetTaskForImageAndRefInternal(image, TracingInfo(0, TilePriority::NOW),
                                       TaskType::kOutOfRaster);
}

ImageDecodeCache::TaskResult
SoftwareImageDecodeCache::GetTaskForImageAndRefInternal(
    const DrawImage& image,
    const TracingInfo& tracing_info,
    TaskType task_type) {
  CacheKey key = CacheKey::FromDrawImage(
      image, GetColorTypeForPaintImage(image.target_color_params(),
                                       image.paint_image()));
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::GetTaskForImageAndRefInternal", "key",
               key.ToString());

  // If the target size is empty, we can skip this image during draw (and thus
  // we don't need to decode it or ref it).
  if (key.target_size().IsEmpty())
    return TaskResult(/*need_unref=*/false, /*is_at_raster_decode=*/false,
                      /*can_do_hardware_accelerated_decode=*/false);

  if (!UseCacheForDrawImage(image))
    return TaskResult(/*need_unref=*/false, /*is_at_raster_decode=*/false,
                      /*can_do_hardware_accelerated_decode=*/false);

  base::AutoLock lock(lock_);

  bool new_image_fits_in_memory =
      locked_images_budget_.AvailableMemoryBytes() >= key.locked_bytes();

  // Get or generate the cache entry.
  auto decoded_it = decoded_images_.Get(key);
  CacheEntry* cache_entry = nullptr;
  if (decoded_it == decoded_images_.end()) {
    // There is no reason to create a new entry if we know it won't fit anyway.
    if (!new_image_fits_in_memory)
      return TaskResult(/*need_unref=*/false, /*is_at_raster_decode=*/true,
                        /*can_do_hardware_accelerated_decode=*/false);
    cache_entry = AddCacheEntry(key);
    if (task_type == TaskType::kOutOfRaster) {
      cache_entry->mark_out_of_raster();
    }
  } else {
    cache_entry = decoded_it->second.get();
  }
  DCHECK(cache_entry);

  if (!cache_entry->is_budgeted) {
    if (!new_image_fits_in_memory) {
      // We don't need to ref anything here because this image will be at
      // raster.
      return TaskResult(/*need_unref=*/false, /*is_at_raster_decode=*/true,
                        /*can_do_hardware_accelerated_decode=*/false);
    }
    AddBudgetForImage(key, cache_entry);
  }
  DCHECK(cache_entry->is_budgeted);

  // The rest of the code will return either true or a task, so we should ref
  // the image once now for the caller to unref.
  ++cache_entry->ref_count;

  // If we already have a locked entry, then we can just use that. Otherwise
  // we'll have to create a task.
  if (cache_entry->is_locked)
    return TaskResult(/*need_unref=*/true, /*is_at_raster_decode=*/false,
                      /*can_do_hardware_accelerated_decode=*/false);

  scoped_refptr<TileTask>& task = task_type == TaskType::kInRaster
                                      ? cache_entry->in_raster_task
                                      : cache_entry->out_of_raster_task;
  if (!task) {
    // Ref image once for the decode task.
    ++cache_entry->ref_count;
    task = base::MakeRefCounted<SoftwareImageDecodeTaskImpl>(
        this, key, image.paint_image(), task_type, tracing_info);
  }
  return TaskResult(task, /*can_do_hardware_accelerated_decode=*/false);
}

void SoftwareImageDecodeCache::AddBudgetForImage(const CacheKey& key,
                                                 CacheEntry* entry) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::AddBudgetForImage", "key",
               key.ToString());

  DCHECK(!entry->is_budgeted);
  DCHECK_GE(locked_images_budget_.AvailableMemoryBytes(), key.locked_bytes());
  locked_images_budget_.AddUsage(key.locked_bytes());
  entry->is_budgeted = true;
}

void SoftwareImageDecodeCache::RemoveBudgetForImage(const CacheKey& key,
                                                    CacheEntry* entry) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::RemoveBudgetForImage", "key",
               key.ToString());

  DCHECK(entry->is_budgeted);
  locked_images_budget_.SubtractUsage(key.locked_bytes());
  entry->is_budgeted = false;
}

void SoftwareImageDecodeCache::UnrefImage(const DrawImage& image) {
  const CacheKey& key = CacheKey::FromDrawImage(
      image, GetColorTypeForPaintImage(image.target_color_params(),
                                       image.paint_image()));
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::UnrefImage", "key", key.ToString());

  base::AutoLock lock(lock_);
  UnrefImage(key);
}

void SoftwareImageDecodeCache::UnrefImage(const CacheKey& key) {
  auto decoded_image_it = decoded_images_.Peek(key);
  CHECK(decoded_image_it != decoded_images_.end(), base::NotFatalUntil::M130);
  auto* entry = decoded_image_it->second.get();
  DCHECK_GT(entry->ref_count, 0);
  if (--entry->ref_count == 0) {
    if (entry->is_budgeted)
      RemoveBudgetForImage(key, entry);
    if (entry->is_locked)
      entry->Unlock();

    ReduceCacheUsageUntilWithinLimit(max_items_in_cache_);
  }
}

SoftwareImageDecodeCache::TaskProcessingResult
SoftwareImageDecodeCache::DecodeImageInTask(const CacheKey& key,
                                            const PaintImage& paint_image,
                                            TaskType task_type) {
  TRACE_EVENT1("cc,benchmark", "SoftwareImageDecodeCache::DecodeImageInTask",
               "key", key.ToString());
  base::AutoLock lock(lock_);

  auto image_it = decoded_images_.Peek(key);
  CHECK(image_it != decoded_images_.end(), base::NotFatalUntil::M130);
  auto* cache_entry = image_it->second.get();
  // These two checks must be true because we're running this from a task, which
  // means that we've budgeted this entry when we got the task and the ref count
  // is also held by the task (released in OnTaskCompleted).
  DCHECK_GT(cache_entry->ref_count, 0);
  DCHECK(cache_entry->is_budgeted);

  TaskProcessingResult result =
      DecodeImageIfNecessary(key, paint_image, cache_entry);
  DCHECK(cache_entry->decode_failed || cache_entry->is_locked);
  return result;
}

SoftwareImageDecodeCache::TaskProcessingResult
SoftwareImageDecodeCache::DecodeImageIfNecessary(const CacheKey& key,
                                                 const PaintImage& paint_image,
                                                 CacheEntry* entry) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::DecodeImageIfNecessary", "key",
               key.ToString());
  DCHECK_GT(entry->ref_count, 0);

  if (key.target_size().IsEmpty())
    entry->decode_failed = true;

  if (entry->decode_failed)
    return TaskProcessingResult::kCancelled;

  if (entry->memory) {
    if (entry->is_locked)
      return TaskProcessingResult::kLockOnly;

    bool lock_succeeded = entry->Lock();
    if (lock_succeeded)
      return TaskProcessingResult::kLockOnly;
  }

  std::unique_ptr<CacheEntry> local_cache_entry;
  // If we can use the original decode, we'll definitely need a decode.
  if (key.type() == CacheKey::kOriginal) {
    base::AutoUnlock release(lock_);
    local_cache_entry = Utils::DoDecodeImage(
        key, paint_image,
        GetColorTypeForPaintImage(key.target_color_params(), paint_image),
        generator_client_id_,
        base::BindOnce(&SoftwareImageDecodeCache::ClearCache,
                       base::Unretained(this)));
  } else {
    // Attempt to find a cached decode to generate a scaled/subrected decode
    // from.
    std::optional<CacheKey> candidate_key = FindCachedCandidate(key);

    SkISize desired_size = gfx::SizeToSkISize(key.target_size());
    const bool should_decode_to_scale =
        // Prefer scaling from a cached decode instead of performing another
        // decode to the desired size.
        !candidate_key &&
        // We need the original decode to subrect before scaling, if a subrect
        // is requested.
        key.src_rect() ==
            gfx::Rect(paint_image.width(), paint_image.height()) &&
        // Note that in the case where we can't decode to the exact desired
        // size, but a size lower than the original, it would be better to
        // decode to that size and then scale to the desired size. But this
        // should be rare in practice, since we only decode to mip levels.
        paint_image.GetSupportedDecodeSize(desired_size) == desired_size;

    // We don't scale and cache the result if nearest neighbor is requested,
    // i.e., the processing type should be kOriginal or kSubrectOriginal. And
    // requesting a subrect already vetoes decode to scale.
    DCHECK(!should_decode_to_scale || !key.is_nearest_neighbor());
    if (should_decode_to_scale) {
      base::AutoUnlock release(lock_);
      local_cache_entry = Utils::DoDecodeImage(
          key, paint_image,
          GetColorTypeForPaintImage(key.target_color_params(), paint_image),
          generator_client_id_,
          base::BindOnce(&SoftwareImageDecodeCache::ClearCache,
                         base::Unretained(this)));
    }

    // Couldn't decode to scale or find a cached candidate. Create the
    // intermediate candidate key required for this decode.
    if (!should_decode_to_scale && !candidate_key) {
      // IMPORTANT: There is a bit of a subtlety here. We would normally want to
      // generate a new candidate with the key.src_rect() as the src_rect. This
      // would ensure that when scaling we won't need to peek pixels, since it's
      // unclear how to adjust the src rect to account for the candidate scale
      // if the candidate came from above.
      //
      // However, if the key type is kSubrectOriginal, then this would generate
      // an exactly same key as we want in the first place, causing infinite
      // recursion. (There is a CHECK guard for this below, since this is a
      // pretty bad case.)
      //
      // Since kSubrectOriginal means we have no scale, to remedy the situation
      // we use the full image rect as the src for this temporary candidate.
      // This way the GenerateCacheEntryFromCandidate() function will simply
      // extract the subset and be done with it.
      auto src_rect =
          key.type() == CacheKey::kSubrectOriginal
              ? SkIRect::MakeWH(paint_image.width(), paint_image.height())
              : gfx::RectToSkIRect(key.src_rect());
      DrawImage candidate_draw_image(
          paint_image, false, src_rect, PaintFlags::FilterQuality::kNone,
          SkM44(), key.frame_key().frame_index(), key.target_color_params());
      candidate_key.emplace(CacheKey::FromDrawImage(
          candidate_draw_image,
          GetColorTypeForPaintImage(key.target_color_params(), paint_image)));
    }

    if (candidate_key) {
      CHECK(*candidate_key != key) << key.ToString();
      auto decoded_draw_image =
          GetDecodedImageForDrawInternal(*candidate_key, paint_image);
      if (!decoded_draw_image.image()) {
        local_cache_entry = nullptr;
      } else {
        base::AutoUnlock release(lock_);
        // IMPORTANT: More subtleties:
        // If the candidate could have used the original decode, that means we
        // need to extractSubset from it. In all other cases, this would have
        // already been done to generate the candidate.
        local_cache_entry = Utils::GenerateCacheEntryFromCandidate(
            key, decoded_draw_image,
            candidate_key->type() == CacheKey::kOriginal,
            GetColorTypeForPaintImage(key.target_color_params(), paint_image));
      }

      // Unref to balance the GetDecodedImageForDrawInternal() call.
      UnrefImage(*candidate_key);
    }
  }

  if (!local_cache_entry) {
    entry->decode_failed = true;
    return TaskProcessingResult::kCancelled;
  }

  // Just in case someone else did this already, just unlock our work.
  // TODO(vmpstr): It's possible to have a pending decode state where the
  // thread would just block on a cv and wait for that decode to finish
  // instead of actually doing the work.
  if (entry->memory) {
    // This would have to be locked because we hold a ref count on the entry. So
    // if someone ever populated the entry with memory, they would not be able
    // to unlock it.
    DCHECK(entry->is_locked);
    // Unlock our local memory though.
    local_cache_entry->Unlock();
  } else {
    local_cache_entry->MoveImageMemoryTo(entry);
    DCHECK(entry->is_locked);
  }

  return TaskProcessingResult::kFullDecode;
}

std::optional<SoftwareImageDecodeCache::CacheKey>
SoftwareImageDecodeCache::FindCachedCandidate(const CacheKey& key) {
  auto image_keys_it = frame_key_to_image_keys_.find(key.frame_key());
  // We know that we must have at least our own |entry| in this list, so it
  // won't be empty.
  CHECK(image_keys_it != frame_key_to_image_keys_.end(),
        base::NotFatalUntil::M130);

  auto& available_keys = image_keys_it->second;
  std::sort(available_keys.begin(), available_keys.end(),
            [](const CacheKey& one, const CacheKey& two) {
              // Return true if |one| scale is less than |two| scale.
              return one.target_size().width() < two.target_size().width() &&
                     one.target_size().height() < two.target_size().height();
            });

  for (auto& available_key : available_keys) {
    // Only consider keys coming from the same src rect, since otherwise the
    // resulting image was extracted using a different src.
    if (available_key.src_rect() != key.src_rect())
      continue;

    // That are at least as big as the required |key|.
    if (available_key.target_size().width() < key.target_size().width() ||
        available_key.target_size().height() < key.target_size().height()) {
      continue;
    }
    auto image_it = decoded_images_.Peek(available_key);
    CHECK(image_it != decoded_images_.end(), base::NotFatalUntil::M130);
    auto* available_entry = image_it->second.get();
    if (available_entry->is_locked || available_entry->Lock()) {
      return available_key;
    }
  }

  return std::nullopt;
}

bool SoftwareImageDecodeCache::UseCacheForDrawImage(
    const DrawImage& draw_image) const {
  PaintImage paint_image = draw_image.paint_image();

  // Software cache doesn't support using texture backed images.
  if (paint_image.IsTextureBacked())
    return false;

  // Lazy generated images need to have their decode cached.
  if (paint_image.IsLazyGenerated())
    return true;

  // Cache images that need to be converted to a non-sRGB color space.
  // TODO(ccameron): Consider caching when any color conversion is required.
  // https://crbug.com/791828
  const gfx::ColorSpace& dst_color_space = draw_image.target_color_space();
  if (dst_color_space.IsValid() &&
      dst_color_space != gfx::ColorSpace::CreateSRGB()) {
    return true;
  }

  return false;
}

ImageDecodeCache::ClientId SoftwareImageDecodeCache::GenerateClientId() {
  ClientId next_client_id = ImageDecodeCache::GenerateClientId();
  // The software decode cache cannot be shared between multiple clients. Thus,
  // this DCHECK helps us to verify the software cache has only a single client
  // that generated a client id for itself only oce.
  DCHECK_EQ(ImageDecodeCache::kDefaultClientId, next_client_id);
  return next_client_id;
}

DecodedDrawImage SoftwareImageDecodeCache::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  DCHECK(UseCacheForDrawImage(draw_image));

  base::AutoLock hold(lock_);
  return GetDecodedImageForDrawInternal(
      CacheKey::FromDrawImage(draw_image, GetColorTypeForPaintImage(
                                              draw_image.target_color_params(),
                                              draw_image.paint_image())),
      draw_image.paint_image());
}

DecodedDrawImage SoftwareImageDecodeCache::GetDecodedImageForDrawInternal(
    const CacheKey& key,
    const PaintImage& paint_image) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::GetDecodedImageForDrawInternal",
               "key", key.ToString());

  auto decoded_it = decoded_images_.Get(key);
  CacheEntry* cache_entry = nullptr;
  if (decoded_it == decoded_images_.end())
    cache_entry = AddCacheEntry(key);
  else
    cache_entry = decoded_it->second.get();

  // We'll definitely ref this cache entry and use it.
  ++cache_entry->ref_count;
  cache_entry->mark_used();

  DecodeImageIfNecessary(key, paint_image, cache_entry);
  auto decoded_image = cache_entry->image();
  if (!decoded_image)
    return DecodedDrawImage();

  auto decoded_draw_image =
      DecodedDrawImage(std::move(decoded_image), nullptr,
                       cache_entry->src_rect_offset(), GetScaleAdjustment(key),
                       GetDecodedFilterQuality(key), cache_entry->is_budgeted);
  return decoded_draw_image;
}

void SoftwareImageDecodeCache::DrawWithImageFinished(
    const DrawImage& image,
    const DecodedDrawImage& decoded_image) {
  DCHECK(UseCacheForDrawImage(image));
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::DrawWithImageFinished", "key",
               CacheKey::FromDrawImage(
                   image, GetColorTypeForPaintImage(image.target_color_params(),
                                                    image.paint_image()))
                   .ToString());
  UnrefImage(image);
}

void SoftwareImageDecodeCache::ReduceCacheUsageUntilWithinLimit(size_t limit) {
  TRACE_EVENT0("cc",
               "SoftwareImageDecodeCache::ReduceCacheUsageUntilWithinLimit");
  for (auto it = decoded_images_.rbegin();
       decoded_images_.size() > limit && it != decoded_images_.rend();) {
    if (it->second->ref_count != 0) {
      ++it;
      continue;
    }

    const CacheKey& key = it->first;
    auto vector_it = frame_key_to_image_keys_.find(key.frame_key());
    auto item_it = base::ranges::find(vector_it->second, key);
    CHECK(item_it != vector_it->second.end(), base::NotFatalUntil::M130);
    vector_it->second.erase(item_it);
    if (vector_it->second.empty())
      frame_key_to_image_keys_.erase(vector_it);

    it = decoded_images_.Erase(it);
  }
}

void SoftwareImageDecodeCache::ReduceCacheUsage() {
  base::AutoLock lock(lock_);
  ReduceCacheUsageUntilWithinLimit(max_items_in_cache_);
}

void SoftwareImageDecodeCache::ClearCache() {
  base::AutoLock lock(lock_);
  ReduceCacheUsageUntilWithinLimit(0);
}

size_t SoftwareImageDecodeCache::GetMaximumMemoryLimitBytes() const {
  return locked_images_budget_.total_limit_bytes();
}

void SoftwareImageDecodeCache::OnImageDecodeTaskCompleted(const CacheKey& key,
                                                          TaskType task_type) {
  base::AutoLock hold(lock_);

  auto image_it = decoded_images_.Peek(key);
  CHECK(image_it != decoded_images_.end(), base::NotFatalUntil::M130);
  CacheEntry* cache_entry = image_it->second.get();
  UMA_HISTOGRAM_BOOLEAN("Compositing.DecodeLCPCandidateImage.Software",
                        key.may_be_lcp_candidate());
  auto& task = task_type == TaskType::kInRaster
                   ? cache_entry->in_raster_task
                   : cache_entry->out_of_raster_task;
  task = nullptr;

  UnrefImage(key);
}

bool SoftwareImageDecodeCache::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);

  if (args.level_of_detail == MemoryDumpLevelOfDetail::kBackground) {
    std::string dump_name = base::StringPrintf(
        "cc/image_memory/cache_0x%" PRIXPTR, reinterpret_cast<uintptr_t>(this));
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar("locked_size", MemoryAllocatorDump::kUnitsBytes,
                    locked_images_budget_.GetCurrentUsageSafe());
  } else {
    for (const auto& image_pair : decoded_images_) {
      int image_id = static_cast<int>(image_pair.first.frame_key().hash());
      CacheEntry* entry = image_pair.second.get();
      DCHECK(entry);
      // We might not have memory for this cache entry, depending on where in
      // the CacheEntry lifecycle we are. If we don't have memory, then we don't
      // have to record it in the dump.
      if (!entry->memory)
        continue;

      std::string dump_name = base::StringPrintf(
          "cc/image_memory/cache_0x%" PRIXPTR "/%s/image_%" PRIu64 "_id_%d",
          reinterpret_cast<uintptr_t>(this),
          entry->is_budgeted ? "budgeted" : "at_raster", entry->tracing_id(),
          image_id);
      // CreateMemoryAllocatorDump will automatically add tracking values for
      // the total size. We also add a "locked_size" below.
      MemoryAllocatorDump* dump =
          entry->memory->CreateMemoryAllocatorDump(dump_name.c_str(), pmd);
      DCHECK(dump);
      size_t locked_bytes =
          entry->is_locked ? image_pair.first.locked_bytes() : 0u;
      dump->AddScalar("locked_size", MemoryAllocatorDump::kUnitsBytes,
                      locked_bytes);
    }
  }

  // Memory dump can't fail, always return true.
  return true;
}

SoftwareImageDecodeCache::CacheEntry* SoftwareImageDecodeCache::AddCacheEntry(
    const CacheKey& key) {
  frame_key_to_image_keys_[key.frame_key()].push_back(key);
  auto it = decoded_images_.Put(key, std::make_unique<CacheEntry>());
  it->second.get()->mark_cached();
  return it->second.get();
}

size_t SoftwareImageDecodeCache::GetNumCacheEntriesForTesting() {
  base::AutoLock lock(lock_);
  return decoded_images_.size();
}
size_t SoftwareImageDecodeCache::GetMaxNumCacheEntriesForTesting() {
  return GetNormalMaxItemsInCacheForSoftware();
}

SkColorType SoftwareImageDecodeCache::GetColorTypeForPaintImage(
    const TargetColorParams& target_color_params,
    const PaintImage& paint_image) {
  const gfx::ColorSpace& target_color_space = target_color_params.color_space;
  // TODO(crbug.com/40128725): Once we have access to the display's buffer
  // format via gfx::DisplayColorSpaces, we should also do this for HBD images.
  // Do not decode an image to F16 unless the PaintImage reports that its type
  // is F16. Otherwise, image decode will fail.
  // https://crbug.com/1488786
  if (paint_image.GetColorType() == kRGBA_F16_SkColorType &&
      paint_image.GetContentColorUsage() == gfx::ContentColorUsage::kHDR &&
      target_color_space.IsHDR()) {
    return kRGBA_F16_SkColorType;
  }
  return color_type_;
}

// MemoryBudget ----------------------------------------------------------------
SoftwareImageDecodeCache::MemoryBudget::MemoryBudget(size_t limit_bytes)
    : limit_bytes_(limit_bytes), current_usage_bytes_(0u) {}

size_t SoftwareImageDecodeCache::MemoryBudget::AvailableMemoryBytes() const {
  size_t usage = GetCurrentUsageSafe();
  return usage >= limit_bytes_ ? 0u : (limit_bytes_ - usage);
}

void SoftwareImageDecodeCache::MemoryBudget::AddUsage(size_t usage) {
  current_usage_bytes_ += usage;
}

void SoftwareImageDecodeCache::MemoryBudget::SubtractUsage(size_t usage) {
  DCHECK_GE(current_usage_bytes_.ValueOrDefault(0u), usage);
  current_usage_bytes_ -= usage;
}

void SoftwareImageDecodeCache::MemoryBudget::ResetUsage() {
  current_usage_bytes_ = 0;
}

size_t SoftwareImageDecodeCache::MemoryBudget::GetCurrentUsageSafe() const {
  return current_usage_bytes_.ValueOrDie();
}

}  // namespace cc
