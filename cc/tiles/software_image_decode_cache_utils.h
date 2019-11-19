// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_UTILS_H_
#define CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_UTILS_H_

#include "base/memory/discardable_memory.h"
#include "base/memory/scoped_refptr.h"
#include "cc/cc_export.h"
#include "cc/paint/decoded_draw_image.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/paint_image.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/image_decode_cache_utils.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class SoftwareImageDecodeCacheUtils {
 private:
  // The following should only be accessed by the software image cache.
  friend class SoftwareImageDecodeCache;

  // CacheKey is a class that gets a cache key out of a given draw
  // image. That is, this key uniquely identifies an image in the cache. Note
  // that it's insufficient to use SkImage's unique id, since the same image can
  // appear in the cache multiple times at different scales and filter
  // qualities.
  class CC_EXPORT CacheKey {
   public:
    // Enum indicating the type of processing to do for this key:
    // kOriginal - use the original decode without any subrecting or scaling.
    // kSubrectOriginal - extract a subrect from the original decode but do not
    //                    scale it.
    // kSubrectAndScale - extract a subrect (if needed) from the original decode
    //                    and scale it.
    enum ProcessingType { kOriginal, kSubrectOriginal, kSubrectAndScale };

    static CacheKey FromDrawImage(const DrawImage& image,
                                  SkColorType color_type);

    CacheKey(const CacheKey& other);

    bool operator==(const CacheKey& other) const {
      // The frame_key always has to be the same. However, after that all
      // original decodes are the same, so if we can use the original decode,
      // return true. If not, then we have to compare every field.
      // |nearest_neighbor_| is not compared below since it is not used for
      // scaled decodes and does not affect the contents of the cache entry
      // (just passed to skia for the filtering to be done at raster time).
      DCHECK(!is_nearest_neighbor_ || type_ != kSubrectAndScale);
      return frame_key_ == other.frame_key_ && type_ == other.type_ &&
             target_color_space_ == other.target_color_space_ &&
             (type_ == kOriginal || (src_rect_ == other.src_rect_ &&
                                     target_size_ == other.target_size_));
    }

    bool operator!=(const CacheKey& other) const { return !(*this == other); }

    const PaintImage::FrameKey& frame_key() const { return frame_key_; }
    PaintImage::Id stable_id() const { return stable_id_; }
    ProcessingType type() const { return type_; }
    bool is_nearest_neighbor() const { return is_nearest_neighbor_; }
    gfx::Rect src_rect() const { return src_rect_; }
    gfx::Size target_size() const { return target_size_; }
    const gfx::ColorSpace& target_color_space() const {
      return target_color_space_;
    }

    size_t get_hash() const { return hash_; }

    // Helper to figure out how much memory the locked image represented by this
    // key would take.
    size_t locked_bytes() const {
      // TODO(vmpstr): Handle formats other than RGBA.
      base::CheckedNumeric<size_t> result = 4;
      result *= target_size_.width();
      result *= target_size_.height();
      return result.ValueOrDefault(std::numeric_limits<size_t>::max());
    }

    std::string ToString() const;

   private:
    CacheKey(PaintImage::FrameKey frame_key,
             PaintImage::Id stable_id,
             ProcessingType type,
             bool is_nearest_neighbor,
             const gfx::Rect& src_rect,
             const gfx::Size& size,
             const gfx::ColorSpace& target_color_space);

    PaintImage::FrameKey frame_key_;
    // The stable id is does not factor into the cache key's value for hashing
    // and comparison (as it is redundant). It is only used to look up other
    // cache entries of the same stable id.
    PaintImage::Id stable_id_;
    ProcessingType type_;
    bool is_nearest_neighbor_;
    gfx::Rect src_rect_;
    gfx::Size target_size_;
    gfx::ColorSpace target_color_space_;
    size_t hash_;
  };

  struct CacheKeyHash {
    size_t operator()(const CacheKey& key) const { return key.get_hash(); }
  };

  // CacheEntry is a convenience storage for discardable memory. It can also
  // construct an image out of SkImageInfo and stored discardable memory.
  class CC_EXPORT CacheEntry {
   public:
    CacheEntry();
    CacheEntry(const SkImageInfo& info,
               std::unique_ptr<base::DiscardableMemory> memory,
               const SkSize& src_rect_offset);
    ~CacheEntry();

    void MoveImageMemoryTo(CacheEntry* entry);

    sk_sp<SkImage> image() const {
      if (!memory)
        return nullptr;
      DCHECK(is_locked);
      return image_;
    }
    const SkSize& src_rect_offset() const { return src_rect_offset_; }

    bool Lock();
    void Unlock();

    // An ID which uniquely identifies this CacheEntry within the image decode
    // cache. Used in memory tracing.
    uint64_t tracing_id() const { return tracing_id_; }
    // Mark this image as being used in either a draw or as a source for a
    // scaled image. Either case represents this decode as being valuable and
    // not wasted.
    void mark_used() { usage_stats_.used = true; }
    void mark_cached() { cached_ = true; }
    void mark_out_of_raster() { usage_stats_.first_lock_out_of_raster = true; }

    // Since this is an inner class, we expose these variables publicly for
    // simplicity.
    // TODO(vmpstr): A good simple clean-up would be to rethink this class
    // and its interactions to instead expose a few functions which would also
    // facilitate easier DCHECKs.
    int ref_count = 0;
    bool decode_failed = false;
    bool is_locked = false;
    bool is_budgeted = false;

    scoped_refptr<TileTask> in_raster_task;
    scoped_refptr<TileTask> out_of_raster_task;

    std::unique_ptr<base::DiscardableMemory> memory;

   private:
    struct UsageStats {
      // We can only create a decoded image in a locked state, so the initial
      // lock count is 1.
      int lock_count = 1;
      bool used = false;
      bool last_lock_failed = false;
      bool first_lock_wasted = false;
      bool first_lock_out_of_raster = false;
    };

    SkImageInfo image_info_;
    sk_sp<SkImage> image_;
    SkSize src_rect_offset_;
    uint64_t tracing_id_;
    UsageStats usage_stats_;
    // Indicates whether this entry was ever in the cache.
    bool cached_ = false;
  };

  static std::unique_ptr<CacheEntry> DoDecodeImage(
      const CacheKey& key,
      const PaintImage& image,
      SkColorType color_type,
      PaintImage::GeneratorClientId client_id);
  static std::unique_ptr<CacheEntry> GenerateCacheEntryFromCandidate(
      const CacheKey& key,
      const DecodedDrawImage& candidate,
      bool needs_extract_subset,
      SkColorType color_type);
};

}  // namespace cc

#endif  // CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_UTILS_H_
