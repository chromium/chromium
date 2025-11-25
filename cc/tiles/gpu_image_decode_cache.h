// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_GPU_IMAGE_DECODE_CACHE_H_
#define CC_TILES_GPU_IMAGE_DECODE_CACHE_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/lru_cache.h"
#include "base/logging.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/cc_export.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/tiles/image_decode_cache.h"

namespace viz {
class RasterContextProvider;
}

namespace cc {

class ColorFilter;
class RasterDarkModeFilter;

// OVERVIEW:
//
// GpuImageDecodeCache handles the decode and upload of images that will
// be used by Skia's GPU raster path. It also maintains a cache of these
// decoded/uploaded images for later re-use.
//
// Generally, when an image is required for raster, GpuImageDecodeCache
// creates two tasks, one to decode the image, and one to upload the image to
// the GPU. These tasks are completed before the raster task which depends on
// the image. We need to separate decode and upload tasks, as decode can occur
// simultaneously on multiple threads, while upload requires the GL context
// lock so it must happen on our non-concurrent raster thread.
//
// Decoded and Uploaded image data share a single cache entry. Depending on how
// far we've progressed, this cache entry may contain CPU-side decoded data,
// GPU-side uploaded data, or both. CPU-side decoded data is stored in software
// discardable memory and is only locked for short periods of time (until the
// upload completes). Uploaded GPU data is stored in GPU discardable memory and
// remains locked for the duration of the raster tasks which depend on it.
//
// In cases where the size of locked GPU images exceeds our working set limits,
// we operate in an "at-raster" mode. In this mode, there are no decode/upload
// tasks, and images are decoded/uploaded as needed, immediately before being
// used in raster. Cache entries for at-raster tasks are marked as such, which
// prevents future tasks from taking a dependency on them and extending their
// lifetime longer than is necessary.
//
// RASTER-SCALE CACHING:
//
// In order to save memory, images which are going to be scaled may be uploaded
// at lower than original resolution. In these cases, we may later need to
// re-upload the image at a higher resolution. To handle multiple images of
// different scales being in use at the same time, we have a two-part caching
// system.
//
// The first cache, |persistent_cache_|, stores one ImageData per image id.
// These ImageDatas are not necessarily associated with a given DrawImage, and
// are saved (persisted) even when their ref-count reaches zero (assuming they
// fit in the current memory budget). This allows for future re-use of image
// resources.
//
// The second cache, |in_use_cache_|, stores one image data per DrawImage -
// this may be the same ImageData that is in the persistent_cache_.  These
// cache entries are more transient and are deleted as soon as all refs to the
// given DrawImage are released (the image is no longer in-use).
//
// For examples of raster-scale caching, see https://goo.gl/0zCd9Z
//
// REF COUNTING:
//
// In dealing with the two caches in GpuImageDecodeCache, there are three
// ref-counting concepts in use:
//   1) ImageData upload/decode ref-counts.
//      These ref-counts represent the overall number of references to the
//      upload or decode portion of an ImageData. These ref-counts control
//      both whether the upload/decode data can be freed, as well as whether an
//      ImageData can be removed from the |persistent_cache_|. ImageDatas are
//      only removed from the |persistent_cache_| if their upload/decode
//      ref-counts are zero or if they are orphaned and replaced by a new entry.
//   2) InUseCacheEntry ref-counts.
//      These ref-counts represent the number of references to an
//      InUseCacheEntry from a specific DrawImage. When the InUseCacheEntry's
//      ref-count reaches 0 it will be deleted.
//   3) scoped_refptr ref-counts.
//      Because both the persistent_cache_ and the in_use_cache_ point at the
//      same ImageDatas (and may need to keep these ImageDatas alive independent
//      of each other), they hold ImageDatas by scoped_refptr. The scoped_refptr
//      keeps an ImageData alive while it is present in either the
//      |persistent_cache_| or |in_use_cache_|.
class CC_EXPORT GpuImageDecodeCache
    : public ImageDecodeCache,
      public base::trace_event::MemoryDumpProvider,
      public base::MemoryPressureListener {
 public:
  explicit GpuImageDecodeCache(viz::RasterContextProvider* context,
                               SkColorType color_type,
                               size_t max_working_set_bytes,
                               int max_texture_size,
                               RasterDarkModeFilter* const dark_mode_filter);
  ~GpuImageDecodeCache() override;

  static base::TimeDelta get_purge_interval();
  static base::TimeDelta get_max_purge_age();

  // ImageDecodeCache overrides.

  // Finds the existing uploaded image for the provided DrawImage. Creates an
  // upload task to upload the image if an existing image does not exist.
  // See |GetTaskForImageAndRefInternal| to learn about the |client_id|.
  TaskResult GetTaskForImageAndRef(ClientId client_id,
                                   const DrawImage& image,
                                   const TracingInfo& tracing_info) override;
  // See |GetTaskForImageAndRefInternal| to learn about the |client_id|.
  TaskResult GetOutOfRasterDecodeTaskForImageAndRef(ClientId client_id,
                                                    const DrawImage& image,
                                                    bool speculative) override;
  void UnrefImage(const DrawImage& image) override;
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& draw_image) override;
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image) override;
  void ReduceCacheUsage() override;
  void SetShouldAggressivelyFreeResources(
      bool aggressively_free_resources) override;
  void ClearCache() override;
  size_t GetMaximumMemoryLimitBytes() const override;
  bool UseCacheForDrawImage(const DrawImage& image) const override;
  void RecordStats() override;

  // MemoryDumpProvider overrides.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // TODO(gyuyoung): OnMemoryPressure is deprecated. So this should be removed
  // when the memory coordinator is enabled by default.
  void OnMemoryPressure(base::MemoryPressureLevel level) override;

  // Called by Decode / Upload tasks.
  void DecodeImageInTask(const DrawImage& image, TaskType task_type);
  void UploadImageInTask(const DrawImage& image);

  // Called by Decode / Upload tasks when tasks are finished.
  void OnImageDecodeTaskCompleted(const DrawImage& image,
                                  TaskType task_type,
                                  ClientId client_id);
  void OnImageUploadTaskCompleted(const DrawImage& image, ClientId client_id);

  bool SupportsColorSpaceConversion() const;

  // For testing only.
  void SetWorkingSetLimitsForTesting(size_t bytes_limit, size_t items_limit) {
    base::AutoLock locker(lock_);
    max_working_set_bytes_ = bytes_limit;
    max_working_set_items_ = items_limit;
  }
  size_t GetWorkingSetBytesForTesting() const {
    base::AutoLock locker(lock_);
    return working_set_bytes_;
  }
  size_t GetNumCacheEntriesForTesting() const {
    base::AutoLock locker(lock_);
    return persistent_cache_.size();
  }
  size_t GetInUseCacheEntriesForTesting() const {
    base::AutoLock locker(lock_);
    return in_use_cache_.size();
  }
  size_t GetDrawImageSizeForTesting(const DrawImage& image);
  void SetImageDecodingFailedForTesting(const DrawImage& image);
  bool DiscardableIsLockedForTesting(const DrawImage& image);
  bool IsInInUseCacheForTesting(const DrawImage& image) const;
  bool IsInPersistentCacheForTesting(const DrawImage& image) const;
  sk_sp<SkImage> GetSWImageDecodeForTesting(const DrawImage& image);
  size_t GetDarkModeImageCacheSizeForTesting(const DrawImage& draw_image);
  size_t paint_image_entries_count_for_testing() const {
    base::AutoLock locker(lock_);
    return paint_image_entries_.size();
  }
  bool NeedsDarkModeFilterForTesting(const DrawImage& draw_image);

  bool HasPendingPurgeTaskForTesting() const {
    base::AutoLock locker(lock_);
    return has_pending_purge_task();
  }

  size_t ids_pending_deletion_count_for_testing() const {
    return ids_pending_deletion_.size();
  }

  // Updating the |last_use| field of the associated |ImageData|.
  void TouchCacheEntryForTesting(const DrawImage& draw_image)
      LOCKS_EXCLUDED(lock_);

  bool AcquireContextLockForTesting();
  void ReleaseContextLockForTesting();

 private:
  using ImageTaskMap = base::flat_map<ClientId, scoped_refptr<TileTask>>;

  // Stores stats tracked by both DecodedImageData and UploadedImageData.
  struct ImageDataBase {
    ImageDataBase();
    ~ImageDataBase();

    bool is_locked() const { return is_locked_; }
    void OnSetLockedData(bool out_of_raster);
    void OnResetData();
    void OnLock();
    void OnUnlock();
    void mark_used() {
      DCHECK(is_locked_);
      usage_stats_.used = true;
    }

    uint32_t ref_count = 0;
    // If non-null, this is the pending task to populate this data.
    ImageTaskMap task_map;

   protected:
    using YUVSkImages = std::array<sk_sp<SkImage>, kNumYUVPlanes>;

    struct UsageStats {
      int lock_count = 1;
      bool used = false;
      bool first_lock_out_of_raster = false;
      bool first_lock_wasted = false;
    };

    // Returns the usage state (see cc file) for histogram logging.
    int UsageState() const;

    bool is_locked_ = false;
    UsageStats usage_stats_;
  };

  // Stores the CPU-side decoded bits and SkImage representation of a single
  // image (either the default or gainmap image).
  struct DecodedAuxImageData {
    // Initialize `data` and make `images` point to `rgba_pixmap` or
    // `yuva_pixmaps`, which must be backed by `data`.
    DecodedAuxImageData();
    DecodedAuxImageData(const SkPixmap& rgba_pixmap,
                        std::unique_ptr<base::DiscardableMemory> data);
    DecodedAuxImageData(const SkYUVAPixmaps& yuva_pixmaps,
                        std::unique_ptr<base::DiscardableMemory> data);
    DecodedAuxImageData(const DecodedAuxImageData&) = delete;
    DecodedAuxImageData(DecodedAuxImageData&&);
    DecodedAuxImageData& operator=(const DecodedAuxImageData&) = delete;
    DecodedAuxImageData& operator=(DecodedAuxImageData&&);
    ~DecodedAuxImageData();

    // Return true if all members are reset.
    bool IsEmpty() const;

    // Release `data` and all entries in `images` and `pixmaps`.
    void ResetData();

    // Check that images are non-nullptr only where pixmaps are non-empty.
    void ValidateImagesMatchPixmaps() const {
      for (int i = 0; i < SkYUVAInfo::kMaxPlanes; ++i) {
        DCHECK_EQ(images[i] == nullptr, pixmaps[i].dimensions().isEmpty());
      }
    }

    std::unique_ptr<base::DiscardableMemory> data;
    std::array<sk_sp<SkImage>, SkYUVAInfo::kMaxPlanes> images;
    std::array<SkPixmap, SkYUVAInfo::kMaxPlanes> pixmaps;
  };

  // Stores the CPU-side decoded bits of an image and supporting fields.
  struct DecodedImageData : public ImageDataBase {
    explicit DecodedImageData(bool is_bitmap_backed);
    ~DecodedImageData();

    bool Lock();
    void Unlock();

    void SetLockedData(
        base::span<DecodedAuxImageData, kAuxImageCount> aux_image_data,
        bool out_of_raster);
    void ResetData();
    bool HasData() const {
      for (const auto& aux_image_data : aux_image_data_) {
        if (aux_image_data.data) {
          return true;
        }
      }
      return false;
    }
    base::DiscardableMemory* data(AuxImage aux_image) const {
      return aux_image_data_[AuxImageIndex(aux_image)].data.get();
    }

    void SetBitmapImage(sk_sp<SkImage> image);
    void ResetBitmapImage();

    sk_sp<SkImage> image(int plane, AuxImage aux_image) const {
      DCHECK_LT(plane, SkYUVAInfo::kMaxPlanes);
      if (is_bitmap_backed_) {
        DCHECK_EQ(aux_image, AuxImage::kDefault);
      } else {
        DCHECK(is_locked());
      }
      return aux_image_data_[AuxImageIndex(aux_image)].images[plane];
    }

    base::span<const SkPixmap> pixmaps(AuxImage aux_image) const {
      DCHECK(is_locked() || is_bitmap_backed_);
      return aux_image_data_[AuxImageIndex(aux_image)].pixmaps;
    }

    // Test-only functions.
    sk_sp<SkImage> ImageForTesting() const {
      return aux_image_data_[kAuxImageIndexDefault].images[0];
    }

    bool decode_failure = false;
    // Similar to |task|, but only is generated if there is no associated upload
    // generated for this task (ie, this is an out-of-raster request for decode.
    ImageTaskMap stand_alone_task_map;

    // Dark mode color filter cache.
    struct SkIRectCompare {
      bool operator()(const SkIRect& a, const SkIRect& b) const {
        return a.fLeft < b.fLeft || a.fTop < b.fTop || a.fRight < b.fRight ||
               a.fBottom < b.fBottom;
      }
    };

    base::flat_map<SkIRect, sk_sp<ColorFilter>, SkIRectCompare>
        dark_mode_color_filter_cache;

   private:
    void ReportUsageStats() const;

    const bool is_bitmap_backed_;
    std::array<DecodedAuxImageData, kAuxImageCount> aux_image_data_;
  };

  // Stores the GPU-side image and supporting fields.
  struct UploadedImageData : public ImageDataBase {
    UploadedImageData();
    ~UploadedImageData();

    void SetTransferCacheId(uint32_t id);
    void Reset();

    std::optional<uint32_t> transfer_cache_id() const {
      return transfer_cache_id_;
    }

   private:
    void ReportUsageStats() const;

    std::optional<uint32_t> transfer_cache_id_;
  };

  // A structure to represent either an RGBA or a YUVA image info.
  struct ImageInfo {
    // Initialize `rgba` or `yuva`, and compute `size`.
    ImageInfo();
    explicit ImageInfo(const SkImageInfo& rgba);
    explicit ImageInfo(const SkYUVAPixmapInfo& yuva);
    ImageInfo(const ImageInfo&);
    ImageInfo& operator=(const ImageInfo&);
    ~ImageInfo();

    // At most one of `rgba` or `yuva` may be valid.
    std::optional<SkImageInfo> rgba;
    std::optional<SkYUVAPixmapInfo> yuva;

    // The number of bytes used by this image.
    size_t size = 0;
  };

  struct ImageData : public base::RefCountedThreadSafe<ImageData> {
    ImageData(PaintImage::Id paint_image_id,
              const gfx::ColorSpace& target_color_space,
              PaintFlags::FilterQuality quality,
              int upload_scale_mip_level,
              bool needs_mips,
              bool is_bitmap_backed,
              bool speculative_decode,
              base::span<ImageInfo, kAuxImageCount> image_info);

    bool HasUploadedData() const;
    void ValidateBudgeted() const;

    const ImageInfo& GetImageInfo(AuxImage aux_image) const {
      switch (aux_image) {
        case AuxImage::kDefault:
          return info;
        case AuxImage::kGainmap:
          return gainmap_info;
      }
    }

    // Return the memory that is used by this image when decoded. This should
    // also equal the memory that is used on the GPU when this is uploaded.
    // In some circumstances the GPU memory usage is slightly different (e.g,
    // when a gainmap or HDR tonemapping is applied). This includes the memory
    // used by all auxiliary images.
    size_t GetTotalSize() const;

    bool IsSpeculativeDecode() const {
      return speculative_decode_usage_stats_.has_value();
    }
    bool SpeculativeDecodeHasMatched() const {
      return IsSpeculativeDecode() &&
             speculative_decode_usage_stats_->min_raster_mip_level < INT_MAX;
    }
    void RecordSpeculativeDecodeMatch(int mip_level);
    void RecordSpeculativeDecodeRasterTaskTakeover();

    const PaintImage::Id paint_image_id;
    const gfx::ColorSpace target_color_space;
    PaintFlags::FilterQuality quality;
    int upload_scale_mip_level;
    bool needs_mips = false;
    bool is_bitmap_backed;
    bool is_budgeted = false;
    base::TimeTicks last_use;

    // The RGBA or YUVA image info for the decoded image. The dimensions may be
    // smaller than the original size if the image needs to be downscaled.
    const ImageInfo info;

    // The RGBA or YUVA image info for the decoded gainmap image. This will
    // return false from IsEmpty if and only if the image has a gainmap.
    const ImageInfo gainmap_info;

    // If true, this image is no longer in our |persistent_cache_| and will be
    // deleted as soon as its ref count reaches zero.
    bool is_orphaned = false;

    DecodedImageData decode;
    UploadedImageData upload;

    struct SpeculativeDecodeUsageStats {
      int speculative_decode_mip_level = -1;
      int min_raster_mip_level = INT_MAX;
      bool raster_task_takeover = false;
    };
    std::optional<SpeculativeDecodeUsageStats> speculative_decode_usage_stats_;

   private:
    friend class base::RefCountedThreadSafe<ImageData>;
    ~ImageData();
  };

  // A ref-count and ImageData, used to associate the ImageData with a specific
  // DrawImage in the |in_use_cache_|.
  struct InUseCacheEntry {
    explicit InUseCacheEntry(scoped_refptr<ImageData> image_data);
    InUseCacheEntry(const InUseCacheEntry& other);
    InUseCacheEntry(InUseCacheEntry&& other);
    ~InUseCacheEntry();

    uint32_t ref_count = 0;
    scoped_refptr<ImageData> image_data;
  };

  // Uniquely identifies (without collisions) a specific DrawImage for use in
  // the |in_use_cache_|.
  struct InUseCacheKeyHash;
  struct InUseCacheKey {
    InUseCacheKey(const DrawImage& draw_image, int mip_level);
    int mip_level() const { return upload_scale_mip_level; }
    bool operator==(const InUseCacheKey& other) const;

   private:
    friend struct GpuImageDecodeCache::InUseCacheKeyHash;

    PaintImage::FrameKey frame_key;
    int upload_scale_mip_level;
    PaintFlags::FilterQuality filter_quality;
    gfx::ColorSpace target_color_space;
  };
  struct InUseCacheKeyHash {
    size_t operator()(const InUseCacheKey&) const;
  };

  // All private functions should only be called while holding |lock_|. Some
  // functions also require the |context_| lock. These are indicated by
  // additional comments.

  // Calculate the mip level to upload-scale the image to before uploading. We
  // use mip levels rather than exact scales to increase re-use of scaled
  // images.
  int CalculateUploadScaleMipLevel(const DrawImage& draw_image,
                                   AuxImage aux_image) const;

  InUseCacheKey InUseCacheKeyFromDrawImage(const DrawImage& draw_image) const;

  // Similar to GetTaskForImageAndRef, but gets the dependent decode task
  // rather than the upload task, if necessary.
  scoped_refptr<TileTask> GetImageDecodeTaskAndRef(
      ClientId client_id,
      const DrawImage& image,
      const TracingInfo& tracing_info,
      TaskType task_type) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Note that this function behaves as if it was public (all of the same locks
  // need to be acquired). Uses |client_id| to identify which client created a
  // task as the client run their tasks in different namespaces. The client
  // which ran their task first will execute the task. All the other clients
  // will have their tasks executed as no-op.
  TaskResult GetTaskForImageAndRefInternal(ClientId client_id,
                                           const DrawImage& image,
                                           const TracingInfo& tracing_info,
                                           TaskType task_type,
                                           bool speculative);

  void RefImageDecode(const DrawImage& draw_image,
                      const InUseCacheKey& cache_key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void UnrefImageDecode(const DrawImage& draw_image,
                        const InUseCacheKey& cache_key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void RefImage(const DrawImage& draw_image, const InUseCacheKey& cache_key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void UnrefImageInternal(const DrawImage& draw_image,
                          const InUseCacheKey& cache_key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Called any time the ownership of an object changed. This includes changes
  // to ref-count or to orphaned status.
  void OwnershipChanged(const DrawImage& draw_image, ImageData* image_data)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Ensures that the working set can hold an element of |required_size|,
  // freeing unreferenced cache entries to make room.
  bool EnsureCapacity(size_t required_size) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool CanFitInWorkingSet(size_t size) const EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool ExceedsCacheLimits() const EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void ReduceCacheUsageLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  bool NeedsDarkModeFilter(const DrawImage& draw_image, ImageData* image_data);
  void DecodeImageAndGenerateDarkModeFilterIfNecessary(
      const DrawImage& draw_image,
      ImageData* image_data,
      TaskType task_type) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void DecodeImageIfNecessary(const DrawImage& draw_image,
                              ImageData* image_data,
                              TaskType task_type,
                              bool needs_decode_for_dark_mode)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void GenerateDarkModeFilter(const DrawImage& draw_image,
                              ImageData* image_data)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  scoped_refptr<GpuImageDecodeCache::ImageData> CreateImageData(
      const DrawImage& image,
      bool speculative_decode);
  void WillAddCacheEntry(const DrawImage& draw_image)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns the SkImageInfo for the resulting image and the mip level for
  // upload.
  std::tuple<SkImageInfo, int> CreateImageInfoForDrawImage(
      const DrawImage& draw_image,
      AuxImage aux_image) const;

  // Finds the ImageData that should be used for the given DrawImage. Looks
  // first in the |in_use_cache_|, and then in the |persistent_cache_|.
  ImageData* GetImageDataForDrawImage(
      const DrawImage& image,
      const InUseCacheKey& key,
      bool record_speculative_decode_stats = false)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns true if the given ImageData can be used to draw the specified
  // DrawImage.
  bool IsCompatible(const ImageData* image_data,
                    const DrawImage& draw_image) const;

  // Helper to delete an image and remove it from the cache. Ensures that
  // the image is unlocked and Skia cleanup is handled on the right thread.
  void DeleteImage(ImageData* image_data) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Helper to unlock an image, indicating that it is no longer actively
  // being used. An image must be locked via TryLockImage below before it
  // can be used again.
  void UnlockImage(ImageData* image_data) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Attempts to lock an image for use. If locking fails (the image is deleted
  // on the service side), this function will delete the local reference to the
  // image and return false.
  enum class HaveContextLock { kYes, kNo };
  bool TryLockImage(HaveContextLock have_context_lock,
                    const DrawImage& draw_image,
                    ImageData* data) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Requires that the |context_| lock be held when calling.
  void UploadImageIfNecessary(const DrawImage& draw_image,
                              ImageData* image_data)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Runs pending operations that required the |context_| lock to be held, but
  // were queued up during a time when the |context_| lock was unavailable.
  // These including deleting, unlocking, and locking textures.
  void RunPendingContextThreadOperations() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void CheckContextLockAcquiredIfNecessary();

  sk_sp<SkColorSpace> ColorSpaceForImageDecode(const DrawImage& image) const;

  // |persistent_cache_| represents the long-lived cache, keeping a certain
  // budget of ImageDatas alive even when their ref count reaches zero.
  using PersistentCache = base::HashingLRUCache<PaintImage::FrameKey,
                                                scoped_refptr<ImageData>,
                                                PaintImage::FrameKeyHash>;
  void AddToPersistentCache(const DrawImage& draw_image,
                            scoped_refptr<ImageData> data)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  template <typename Iterator>
  Iterator RemoveFromPersistentCache(Iterator it)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Purges any old entries from the PersistentCache if the feature to enable
  // this behavior is turned on.
  void MaybePurgeOldCacheEntries() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void PostPurgeOldCacheEntriesTask() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Returns true iff we were able to flush the pending work on the GPU side.
  bool DoPurgeOldCacheEntries(base::TimeDelta max_age)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Tries to flush pending work on GPU side. Returns true iff we succeeded.
  bool TryFlushPendingWork() NO_THREAD_SAFETY_ANALYSIS;
  void PurgeOldCacheEntriesCallback() LOCKS_EXCLUDED(lock_);

  static scoped_refptr<TileTask> GetTaskFromMapForClientId(
      const ClientId client_id,
      const ImageTaskMap& task_map);

  bool has_pending_purge_task() const EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return has_pending_purge_task_;
  }

  const SkColorType color_type_;

  raw_ptr<viz::RasterContextProvider> context_;
  int max_texture_size_ = 0;
  const PaintImage::GeneratorClientId generator_client_id_;
  SkYUVAPixmapInfo::SupportedDataTypes yuva_supported_data_types_;
  const bool enable_clipped_image_scaling_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_ = nullptr;

  // All members below this point must only be accessed while holding |lock_|.
  // The exception are const members like |normal_max_cache_bytes_| that can
  // be accessed without a lock since they are thread safe.
  mutable base::Lock lock_;

  bool has_pending_purge_task_ GUARDED_BY(lock_) = false;

  PersistentCache persistent_cache_ GUARDED_BY(lock_);

  // Tracks the total number of bytes of image data represented by the elements
  // in `persistent_cache_`. Must be updated on AddTo/RemoveFromPersistentCache.
  size_t persistent_cache_memory_size_ GUARDED_BY(lock_) = 0;

  struct CacheEntries {
    std::array<PaintImage::ContentId, 2> content_ids = {
        PaintImage::kInvalidContentId, PaintImage::kInvalidContentId};

    // The number of cache entries for a PaintImage. Note that there can be
    // multiple entries per content_id.
    size_t count = 0u;
  };
  // A map of PaintImage::Id to entries for this image in the
  // |persistent_cache_|.
  base::flat_map<PaintImage::Id, CacheEntries> paint_image_entries_
      GUARDED_BY(lock_);

  // |in_use_cache_| represents the in-use (short-lived) cache. Entries are
  // cleaned up as soon as their ref count reaches zero.
  using InUseCache =
      std::unordered_map<InUseCacheKey, InUseCacheEntry, InUseCacheKeyHash>;
  InUseCache in_use_cache_ GUARDED_BY(lock_);

  size_t max_working_set_bytes_ GUARDED_BY(lock_) = 0;
  size_t max_working_set_items_ GUARDED_BY(lock_) = 0;
  size_t working_set_bytes_ GUARDED_BY(lock_) = 0;
  size_t working_set_items_ GUARDED_BY(lock_) = 0;
  bool aggressively_freeing_resources_ GUARDED_BY(lock_) = false;

  // This field is not a raw_ptr<> because of incompatibilities with tracing
  // (TRACE_EVENT*), perfetto::TracedDictionary::Add and gmock/EXPECT_THAT.
  RAW_PTR_EXCLUSION RasterDarkModeFilter* const dark_mode_filter_;

  const sk_sp<SkColorSpace> target_color_space_;

  std::vector<uint32_t> ids_pending_unlock_;
  std::vector<uint32_t> ids_pending_deletion_;

  std::unique_ptr<base::AsyncMemoryPressureListenerRegistration>
      memory_pressure_listener_registration_;
  base::WeakPtrFactory<GpuImageDecodeCache> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_TILES_GPU_IMAGE_DECODE_CACHE_H_
