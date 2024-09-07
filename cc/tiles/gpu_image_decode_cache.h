// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef CC_TILES_GPU_IMAGE_DECODE_CACHE_H_
#define CC_TILES_GPU_IMAGE_DECODE_CACHE_H_

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/lru_cache.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/cc_export.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/tiles/image_decode_cache.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"

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
//
// HARDWARE ACCELERATED DECODES:
//
// In Chrome OS, we have the ability to use specialized hardware to decode
// certain images. Because this requires interacting with drivers, it must be
// done in the GPU process. Therefore, we follow a different path than the usual
// decode -> upload tasks:
//   1) We decide whether to do hardware decode acceleration for an image before
//      we create the decode/upload tasks. Under the hood, this involves parsing
//      the image and checking if it's supported by the hardware decoder
//      according to information advertised by the GPU process. Also, we only
//      allow hardware decoding in OOP-R mode.
//   2) If we do decide to do hardware decoding, we don't create a decode task.
//      Instead, we create only an upload task and store enough state to
//      indicate that the image will go through this hardware accelerated path.
//      The reason that we use the upload task is that we need to hold the
//      context lock in order to schedule the image decode.
//   3) When the upload task runs, we send a request to the GPU process to start
//      the image decode. This is an IPC message that does not require us to
//      wait for the response. Instead, we get a sync token that is signalled
//      when the decode completes. We insert a wait for this sync token right
//      after sending the decode request.
//
// We also handle the more unusual case where images are decoded at raster time.
// The process is similar: we skip the software decode and then request the
// hardware decode in the same way as step (3) above.
//
// Note that the decoded data never makes it back to the renderer. It stays in
// the GPU process. The sync token ensures that any raster work that needs the
// image happens after the decode completes.
class CC_EXPORT GpuImageDecodeCache
    : public ImageDecodeCache,
      public base::trace_event::MemoryDumpProvider {
 public:
  explicit GpuImageDecodeCache(viz::RasterContextProvider* context,
                               bool use_transfer_cache,
                               SkColorType color_type,
                               size_t max_working_set_bytes,
                               int max_texture_size,
                               RasterDarkModeFilter* const dark_mode_filter);
  ~GpuImageDecodeCache() override;

  // Returns the GL texture ID backing the given SkImage.
  static GrGLuint GlIdFromSkImage(const SkImage* image);

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
  TaskResult GetOutOfRasterDecodeTaskForImageAndRef(
      ClientId client_id,
      const DrawImage& image) override;
  void UnrefImage(const DrawImage& image) override;
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& draw_image) override;
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image) override;
  void ReduceCacheUsage() override;
  void SetShouldAggressivelyFreeResources(bool aggressively_free_resources,
                                          bool context_lock_acquired) override;
  void ClearCache() override;
  size_t GetMaximumMemoryLimitBytes() const override;
  bool UseCacheForDrawImage(const DrawImage& image) const override;
  void RecordStats() override;

  // MemoryDumpProvider overrides.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // TODO(gyuyoung): OnMemoryPressure is deprecated. So this should be removed
  // when the memory coordinator is enabled by default.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Called by Decode / Upload tasks.
  void DecodeImageInTask(const DrawImage& image, TaskType task_type);
  void UploadImageInTask(const DrawImage& image);

  // Called by Decode / Upload tasks when tasks are finished.
  void OnImageDecodeTaskCompleted(const DrawImage& image, TaskType task_type);
  void OnImageUploadTaskCompleted(const DrawImage& image);

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
  sk_sp<SkImage> GetUploadedPlaneForTesting(const DrawImage& draw_image,
                                            YUVIndex index);
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
  enum class DecodedDataMode { kGpu, kCpu, kTransferCache };
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
    sk_sp<SkImage> images[SkYUVAInfo::kMaxPlanes];
    SkPixmap pixmaps[SkYUVAInfo::kMaxPlanes];
  };

  // Stores the CPU-side decoded bits of an image and supporting fields.
  struct DecodedImageData : public ImageDataBase {
    explicit DecodedImageData(bool is_bitmap_backed,
                              bool can_do_hardware_accelerated_decode,
                              bool do_hardware_accelerated_decode);
    ~DecodedImageData();

    bool Lock();
    void Unlock();

    void SetLockedData(DecodedAuxImageData aux_image_data[kAuxImageCount],
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

    const SkPixmap* pixmaps(AuxImage aux_image) const {
      DCHECK(is_locked() || is_bitmap_backed_);
      return aux_image_data_[AuxImageIndex(aux_image)].pixmaps;
    }

    bool can_do_hardware_accelerated_decode() const {
      return can_do_hardware_accelerated_decode_;
    }

    bool do_hardware_accelerated_decode() const {
      return do_hardware_accelerated_decode_;
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
    DecodedAuxImageData aux_image_data_[kAuxImageCount];

    // Keeps tracks of images that could go through hardware decode acceleration
    // though they're possibly prevented from doing so because of a disabled
    // feature flag.
    bool can_do_hardware_accelerated_decode_;

    // |do_hardware_accelerated_decode_| keeps track of images that should go
    // through hardware decode acceleration. Currently, this path is intended
    // only for Chrome OS and only for some JPEG images (see
    // https://crbug.com/868400).
    bool do_hardware_accelerated_decode_;
  };

  // Stores the GPU-side image and supporting fields.
  struct UploadedImageData : public ImageDataBase {
    UploadedImageData();
    ~UploadedImageData();

    // If |represents_yuv_image| is true, the method knows not to check for a
    // texture ID for |image|, which would inadvertently flatten it to RGB.
    void SetImage(sk_sp<SkImage> image, bool represents_yuv_image = false);
    void SetYuvImage(sk_sp<SkImage> y_image_input,
                     sk_sp<SkImage> u_image_input,
                     sk_sp<SkImage> v_image_input);
    void SetTransferCacheId(uint32_t id);
    void Reset();

    // If in image mode.
    const sk_sp<SkImage>& image() const {
      DCHECK(mode_ == Mode::kSkImage || mode_ == Mode::kNone);
      return image_;
    }
    const sk_sp<SkImage>& y_image() const {
      return plane_image_internal(YUVIndex::kY);
    }
    const sk_sp<SkImage>& u_image() const {
      return plane_image_internal(YUVIndex::kU);
    }
    const sk_sp<SkImage>& v_image() const {
      return plane_image_internal(YUVIndex::kV);
    }
    GrGLuint gl_id() const {
      DCHECK(mode_ == Mode::kSkImage || mode_ == Mode::kNone);
      return gl_id_;
    }

    GrGLuint gl_y_id() const { return gl_plane_id_internal(YUVIndex::kY); }
    GrGLuint gl_u_id() const { return gl_plane_id_internal(YUVIndex::kU); }
    GrGLuint gl_v_id() const { return gl_plane_id_internal(YUVIndex::kV); }

    // We consider an image to be valid YUV if all planes are non-null.
    bool has_yuv_planes() const {
      if (!image_yuv_planes_) {
        return false;
      }
      auto yuv_planes_rstart = image_yuv_planes_->crbegin() + !is_alpha_;
      auto yuv_planes_rend = image_yuv_planes_->crend();
      // Iterates from end to beginning, skipping alpha plane (verified to be
      // last) if the image is not alpha.
      bool has_existing_planes = std::any_of(yuv_planes_rstart, yuv_planes_rend,
                                             [](auto& it) { return it; });
      bool has_null_planes = std::any_of(yuv_planes_rstart, yuv_planes_rend,
                                         [](auto& it) { return !it; });
      if (has_existing_planes && has_null_planes) {
        DLOG(ERROR) << "Image has a mix of null and decoded planes";
      }
      return has_existing_planes && !has_null_planes;
    }

    // If in transfer cache mode.
    std::optional<uint32_t> transfer_cache_id() const {
      DCHECK(mode_ == Mode::kTransferCache || mode_ == Mode::kNone);
      return transfer_cache_id_;
    }

    void set_unmipped_image(sk_sp<SkImage> image) {
      unmipped_image_ = std::move(image);
    }
    sk_sp<SkImage> take_unmipped_image() {
      DCHECK(!is_locked_);
      return std::move(unmipped_image_);
    }

    void set_unmipped_yuv_images(sk_sp<SkImage> y_image,
                                 sk_sp<SkImage> u_image,
                                 sk_sp<SkImage> v_image) {
      if (!unmipped_yuv_images_) {
        unmipped_yuv_images_ = YUVSkImages();
      }
      unmipped_yuv_images_->at(static_cast<size_t>(YUVIndex::kY)) =
          std::move(y_image);
      unmipped_yuv_images_->at(static_cast<size_t>(YUVIndex::kU)) =
          std::move(u_image);
      unmipped_yuv_images_->at(static_cast<size_t>(YUVIndex::kV)) =
          std::move(v_image);
    }

    sk_sp<SkImage> take_unmipped_y_image() {
      return take_unmipped_yuv_image_internal(YUVIndex::kY);
    }

    sk_sp<SkImage> take_unmipped_u_image() {
      return take_unmipped_yuv_image_internal(YUVIndex::kU);
    }

    sk_sp<SkImage> take_unmipped_v_image() {
      return take_unmipped_yuv_image_internal(YUVIndex::kV);
    }

    sk_sp<SkImage> take_unmipped_yuv_image_internal(const YUVIndex yuv_index) {
      DCHECK(!is_locked_);
      const size_t index = static_cast<size_t>(yuv_index);
      if (unmipped_yuv_images_ && unmipped_yuv_images_->size() > index) {
        return std::move(unmipped_yuv_images_->at(index));
      }
      return nullptr;
    }

   private:
    // Used for internal DCHECKs only.
    enum class Mode {
      kNone,
      kSkImage,
      kTransferCache,
    };

    void ReportUsageStats() const;

    const sk_sp<SkImage>& plane_image_internal(const YUVIndex yuv_index) const {
      DCHECK(mode_ == Mode::kSkImage || mode_ == Mode::kNone);
      DCHECK(image_yuv_planes_);
      const size_t index = static_cast<size_t>(yuv_index);
      DCHECK_GT(image_yuv_planes_->size(), index)
          << "Requested reference to a plane_id that is not set";
      return image_yuv_planes_->at(index);
    }

    GrGLuint gl_plane_id_internal(const YUVIndex yuv_index) const {
      DCHECK(mode_ == Mode::kSkImage || mode_ == Mode::kNone);
      DCHECK(gl_plane_ids_);
      const size_t index = static_cast<size_t>(yuv_index);
      DCHECK_GT(gl_plane_ids_->size(), index)
          << "Requested GL id for a plane texture that is not uploaded";
      return gl_plane_ids_->at(index);
    }

    Mode mode_ = Mode::kNone;

    // Used if |mode_| == kSkImage.
    // May be null if image not yet uploaded / prepared.
    sk_sp<SkImage> image_;
    std::optional<YUVSkImages> image_yuv_planes_;
    // TODO(crbug.com/40604431): Change after alpha support.
    bool is_alpha_ = false;
    GrGLuint gl_id_ = 0;
    std::optional<std::array<GrGLuint, kNumYUVPlanes>> gl_plane_ids_;

    // Used if |mode_| == kTransferCache.
    std::optional<uint32_t> transfer_cache_id_;

    // The original un-mipped image, for RGBX, or the representative image
    // backed by three planes for YUV. It is retained until it can be safely
    // deleted.
    sk_sp<SkImage> unmipped_image_;
    // Used for YUV decoding and null otherwise.
    std::optional<YUVSkImages> unmipped_yuv_images_;
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
              DecodedDataMode mode,
              const gfx::ColorSpace& target_color_space,
              PaintFlags::FilterQuality quality,
              int upload_scale_mip_level,
              bool needs_mips,
              bool is_bitmap_backed,
              bool can_do_hardware_accelerated_decode,
              bool do_hardware_accelerated_decode,
              ImageInfo image_info[kAuxImageCount]);

    bool IsGpuOrTransferCache() const;
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

    const PaintImage::Id paint_image_id;
    const DecodedDataMode mode;
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
                                           TaskType task_type);

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

  void InsertTransferCacheEntry(
      const ClientImageTransferCacheEntry& image_entry,
      ImageData* image_data) EXCLUSIVE_LOCKS_REQUIRED(lock_);
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
  sk_sp<SkImage> CreateImageFromYUVATexturesInternal(
      const SkImage* uploaded_y_image,
      const SkImage* uploaded_u_image,
      const SkImage* uploaded_v_image,
      const int image_width,
      const int image_height,
      const SkYUVAInfo::PlaneConfig yuva_plane_config,
      const SkYUVAInfo::Subsampling yuva_subsampling,
      const SkYUVColorSpace yuva_color_space,
      sk_sp<SkColorSpace> target_color_space,
      sk_sp<SkColorSpace> decoded_color_space) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  scoped_refptr<GpuImageDecodeCache::ImageData> CreateImageData(
      const DrawImage& image,
      bool allow_hardware_decode);
  void WillAddCacheEntry(const DrawImage& draw_image)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns the SkImageInfo for the resulting image and the mip level for
  // upload.
  std::tuple<SkImageInfo, int> CreateImageInfoForDrawImage(
      const DrawImage& draw_image,
      AuxImage aux_image) const;

  // Finds the ImageData that should be used for the given DrawImage. Looks
  // first in the |in_use_cache_|, and then in the |persistent_cache_|.
  ImageData* GetImageDataForDrawImage(const DrawImage& image,
                                      const InUseCacheKey& key)
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

  // Implementation of UploadImageIfNecessary for each sub-case.
  void UploadImageIfNecessary_TransferCache_HardwareDecode(
      const DrawImage& draw_image,
      ImageData* image_data,
      sk_sp<SkColorSpace> color_space) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void UploadImageIfNecessary_TransferCache_SoftwareDecode(
      const DrawImage& draw_image,
      ImageData* image_data,
      sk_sp<SkColorSpace> decoded_target_colorspace,
      const std::optional<gfx::HDRMetadata>& hdr_metadata,
      sk_sp<SkColorSpace> target_color_space) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void UploadImageIfNecessary_GpuCpu_YUVA(
      const DrawImage& draw_image,
      ImageData* image_data,
      sk_sp<SkImage> uploaded_image,
      skgpu::Mipmapped image_needs_mips,
      sk_sp<SkColorSpace> decoded_target_colorspace,
      sk_sp<SkColorSpace> color_space) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void UploadImageIfNecessary_GpuCpu_RGBA(const DrawImage& draw_image,
                                          ImageData* image_data,
                                          sk_sp<SkImage> uploaded_image,
                                          skgpu::Mipmapped image_needs_mips,
                                          sk_sp<SkColorSpace> color_space)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Flush pending operations on context_->GrContext() for each element of
  // |yuv_images| and then clear the vector.
  void FlushYUVImages(std::vector<sk_sp<SkImage>>* yuv_images)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Runs pending operations that required the |context_| lock to be held, but
  // were queued up during a time when the |context_| lock was unavailable.
  // These including deleting, unlocking, and locking textures.
  void RunPendingContextThreadOperations() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void CheckContextLockAcquiredIfNecessary();

  sk_sp<SkColorSpace> ColorSpaceForImageDecode(const DrawImage& image,
                                               DecodedDataMode mode) const;

  // Helper function to add a memory dump to |pmd| for a single texture
  // identified by |gl_id| with size |bytes| and |locked_size| equal to either
  // |bytes| or 0 depending on whether the texture is currently locked.
  void AddTextureDump(base::trace_event::ProcessMemoryDump* pmd,
                      const std::string& texture_dump_name,
                      const size_t bytes,
                      const GrGLuint gl_id,
                      const size_t locked_size) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Alias each texture of the YUV image entry to its Skia texture counterpart,
  // taking ownership of the memory and preventing double counting.
  //
  // Given |dump_base_name| as the location where single RGB image textures are
  // dumped, this method creates dumps under |pmd| for the planar textures
  // backing |image_data| as subcategories plane_0, plane_1, etc.
  void MemoryDumpYUVImage(base::trace_event::ProcessMemoryDump* pmd,
                          const ImageData* image_data,
                          const std::string& dump_base_name,
                          size_t locked_size) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

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

  // Adds mips to an image if required.
  void UpdateMipsIfNeeded(const DrawImage& draw_image, ImageData* image_data)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  static scoped_refptr<TileTask> GetTaskFromMapForClientId(
      const ClientId client_id,
      const ImageTaskMap& task_map);

  bool has_pending_purge_task() const EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return has_pending_purge_task_;
  }

  const SkColorType color_type_;
  const bool use_transfer_cache_ = false;
  raw_ptr<viz::RasterContextProvider> context_;
  int max_texture_size_ = 0;
  const PaintImage::GeneratorClientId generator_client_id_;
  bool allow_accelerated_jpeg_decodes_ = false;
  bool allow_accelerated_webp_decodes_ = false;
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
    PaintImage::ContentId content_ids[2] = {PaintImage::kInvalidContentId,
                                            PaintImage::kInvalidContentId};

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

  // We can't modify GPU backed SkImages without holding the context lock, so
  // we queue up operations to run the next time the lock is held.
  std::vector<raw_ptr<SkImage, VectorExperimental>>
      images_pending_complete_lock_;
  std::vector<raw_ptr<SkImage, VectorExperimental>> images_pending_unlock_;
  std::vector<sk_sp<SkImage>> images_pending_deletion_;
  // Images that are backed by planar textures must be handled differently
  // to avoid inadvertently flattening to RGB and creating additional textures.
  // See comment in RunPendingContextThreadOperations().
  std::vector<sk_sp<SkImage>> yuv_images_pending_deletion_;
  std::vector<sk_sp<SkImage>> yuv_images_pending_unlock_;
  const sk_sp<SkColorSpace> target_color_space_;

  std::vector<uint32_t> ids_pending_unlock_;
  std::vector<uint32_t> ids_pending_deletion_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  base::WeakPtrFactory<GpuImageDecodeCache> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_TILES_GPU_IMAGE_DECODE_CACHE_H_
