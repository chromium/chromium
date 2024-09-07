// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_H_
#define CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/numerics/safe_math.h"
#include "base/thread_annotations.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/cc_export.h"
#include "cc/paint/draw_image.h"
#include "cc/tiles/image_decode_cache.h"
#include "cc/tiles/software_image_decode_cache_utils.h"

namespace cc {

class CC_EXPORT SoftwareImageDecodeCache
    : public ImageDecodeCache,
      public base::trace_event::MemoryDumpProvider {
 public:
  using Utils = SoftwareImageDecodeCacheUtils;
  using CacheKey = Utils::CacheKey;
  using CacheKeyHash = Utils::CacheKeyHash;

  // Identifies whether a decode task performed decode work, or was fulfilled /
  // failed trivially.
  enum class TaskProcessingResult { kFullDecode, kLockOnly, kCancelled };

  SoftwareImageDecodeCache(SkColorType color_type,
                           size_t locked_memory_limit_bytes);
  ~SoftwareImageDecodeCache() override;

  // ImageDecodeCache overrides.
  // |client_id| is not used by the SoftwareImageDecodeCache for both of these
  // tasks.
  TaskResult GetTaskForImageAndRef(ClientId client_id,
                                   const DrawImage& image,
                                   const TracingInfo& tracing_info) override;
  TaskResult GetOutOfRasterDecodeTaskForImageAndRef(
      ClientId client_id,
      const DrawImage& image) override;
  void UnrefImage(const DrawImage& image) override;
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& image) override;
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image) override;
  void ReduceCacheUsage() override;
  // Software doesn't keep outstanding images pinned, so this is a no-op.
  void SetShouldAggressivelyFreeResources(bool aggressively_free_resources,
                                          bool context_lock_acquired) override {
  }
  void ClearCache() override;
  size_t GetMaximumMemoryLimitBytes() const override;
  bool UseCacheForDrawImage(const DrawImage& image) const override;
  void RecordStats() override {}
  ClientId GenerateClientId() override;

  // Decode the given image and store it in the cache. This is only called by an
  // image decode task from a worker thread.
  TaskProcessingResult DecodeImageInTask(const CacheKey& key,
                                         const PaintImage& paint_image,
                                         TaskType task_type);

  void OnImageDecodeTaskCompleted(const CacheKey& key, TaskType task_type);

  // MemoryDumpProvider overrides.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  size_t GetNumCacheEntriesForTesting();
  size_t GetMaxNumCacheEntriesForTesting();

 private:
  using CacheEntry = Utils::CacheEntry;

  // MemoryBudget is a convenience class for memory bookkeeping and ensuring
  // that we don't go over the limit when pre-decoding.
  class MemoryBudget {
   public:
    explicit MemoryBudget(size_t limit_bytes);

    size_t AvailableMemoryBytes() const;
    void AddUsage(size_t usage);
    void SubtractUsage(size_t usage);
    void ResetUsage();
    size_t total_limit_bytes() const { return limit_bytes_; }
    size_t GetCurrentUsageSafe() const;

   private:
    const size_t limit_bytes_;
    base::CheckedNumeric<size_t> current_usage_bytes_;
  };

  using ImageLRUCache = base::
      HashingLRUCache<CacheKey, std::unique_ptr<CacheEntry>, CacheKeyHash>;

  // Get the decoded draw image for the given key and paint_image. Note that
  // when used internally, we still require that DrawWithImageFinished() is
  // called afterwards.
  DecodedDrawImage GetDecodedImageForDrawInternal(const CacheKey& key,
                                                  const PaintImage& paint_image)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Removes unlocked decoded images until the number of decoded images is
  // reduced within the given limit.
  void ReduceCacheUsageUntilWithinLimit(size_t limit)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Helper method to get the different tasks. Note that this should be used as
  // if it was public (ie, all of the locks need to be properly acquired).
  TaskResult GetTaskForImageAndRefInternal(const DrawImage& image,
                                           const TracingInfo& tracing_info,
                                           TaskType type) LOCKS_EXCLUDED(lock_);

  CacheEntry* AddCacheEntry(const CacheKey& key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  TaskProcessingResult DecodeImageIfNecessary(const CacheKey& key,
                                              const PaintImage& paint_image,
                                              CacheEntry* cache_entry)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void AddBudgetForImage(const CacheKey& key, CacheEntry* entry)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void RemoveBudgetForImage(const CacheKey& key, CacheEntry* entry)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  std::optional<CacheKey> FindCachedCandidate(const CacheKey& key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void UnrefImage(const CacheKey& key) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  SkColorType GetColorTypeForPaintImage(
      const TargetColorParams& target_color_params,
      const PaintImage& paint_image);

  base::Lock lock_;
  // Decoded images and ref counts (predecode path).
  ImageLRUCache decoded_images_ GUARDED_BY(lock_);

  // A map of PaintImage::FrameKey to the ImageKeys for cached decodes of this
  // PaintImage.
  std::unordered_map<PaintImage::FrameKey,
                     std::vector<CacheKey>,
                     PaintImage::FrameKeyHash>
      frame_key_to_image_keys_ GUARDED_BY(lock_);

  // Should be GUARDED_BY(lock_), except that accessing
  // |locked_images_budget_.total_limit_bytes()| is fine without the lock, as
  // it is const and thread safe.
  MemoryBudget locked_images_budget_;

  const SkColorType color_type_;
  const PaintImage::GeneratorClientId generator_client_id_;

  const size_t max_items_in_cache_;
};

}  // namespace cc

#endif  // CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_H_
