// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_H_
#define CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "base/containers/mru_cache.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_math.h"
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

  enum class DecodeTaskType { USE_IN_RASTER_TASKS, USE_OUT_OF_RASTER_TASKS };

  SoftwareImageDecodeCache(SkColorType color_type,
                           size_t locked_memory_limit_bytes,
                           PaintImage::GeneratorClientId generator_client_id);
  ~SoftwareImageDecodeCache() override;

  // ImageDecodeCache overrides.
  TaskResult GetTaskForImageAndRef(const DrawImage& image,
                                   const TracingInfo& tracing_info) override;
  TaskResult GetOutOfRasterDecodeTaskForImageAndRef(
      const DrawImage& image) override;
  void UnrefImage(const DrawImage& image) override;
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& image) override;
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image) override;
  void ReduceCacheUsage() override;
  // Software doesn't keep outstanding images pinned, so this is a no-op.
  void SetShouldAggressivelyFreeResources(
      bool aggressively_free_resources) override {}
  void ClearCache() override;
  size_t GetMaximumMemoryLimitBytes() const override;
  bool UseCacheForDrawImage(const DrawImage& image) const override;

  // Decode the given image and store it in the cache. This is only called by an
  // image decode task from a worker thread.
  void DecodeImageInTask(const CacheKey& key,
                         const PaintImage& paint_image,
                         DecodeTaskType task_type);

  void OnImageDecodeTaskCompleted(const CacheKey& key,
                                  DecodeTaskType task_type);

  // MemoryDumpProvider overrides.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  size_t GetNumCacheEntriesForTesting() const { return decoded_images_.size(); }

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

  using ImageMRUCache = base::
      HashingMRUCache<CacheKey, std::unique_ptr<CacheEntry>, CacheKeyHash>;

  // Actually decode the image. Note that this function can (and should) be
  // called with no lock acquired, since it can do a lot of work. Note that it
  // can also return nullptr to indicate the decode failed.
  std::unique_ptr<CacheEntry> DecodeImageInternal(const CacheKey& key,
                                                  const DrawImage& draw_image);

  // Get the decoded draw image for the given key and paint_image. Note that
  // this function has to be called with no lock acquired, since it will acquire
  // its own locks and might call DecodeImageInternal above. Note that
  // when used internally, we still require that DrawWithImageFinished() is
  // called afterwards.
  DecodedDrawImage GetDecodedImageForDrawInternal(
      const CacheKey& key,
      const PaintImage& paint_image);

  // Removes unlocked decoded images until the number of decoded images is
  // reduced within the given limit.
  void ReduceCacheUsageUntilWithinLimit(size_t limit);

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Helper method to get the different tasks. Note that this should be used as
  // if it was public (ie, all of the locks need to be properly acquired).
  TaskResult GetTaskForImageAndRefInternal(const DrawImage& image,
                                           const TracingInfo& tracing_info,
                                           DecodeTaskType type);

  CacheEntry* AddCacheEntry(const CacheKey& key);

  void DecodeImageIfNecessary(const CacheKey& key,
                              const PaintImage& paint_image,
                              CacheEntry* cache_entry);
  void AddBudgetForImage(const CacheKey& key, CacheEntry* entry);
  void RemoveBudgetForImage(const CacheKey& key, CacheEntry* entry);
  base::Optional<CacheKey> FindCachedCandidate(const CacheKey& key);

  void UnrefImage(const CacheKey& key);

  // The members below this comment can only be accessed if the lock is held to
  // ensure that they are safe to access on multiple threads.
  // The exception is accessing |locked_images_budget_.total_limit_bytes()|,
  // which is const and thread safe.
  base::Lock lock_;

  // Decoded images and ref counts (predecode path).
  ImageMRUCache decoded_images_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // A map of PaintImage::FrameKey to the ImageKeys for cached decodes of this
  // PaintImage.
  std::unordered_map<PaintImage::FrameKey,
                     std::vector<CacheKey>,
                     PaintImage::FrameKeyHash>
      frame_key_to_image_keys_;

  MemoryBudget locked_images_budget_;

  const SkColorType color_type_;
  const PaintImage::GeneratorClientId generator_client_id_;

  size_t max_items_in_cache_;
  // Records the maximum number of items in the cache over the lifetime of the
  // cache. This is updated anytime we are requested to reduce cache usage.
  size_t lifetime_max_items_in_cache_ = 0u;
};

}  // namespace cc

#endif  // CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_H_
