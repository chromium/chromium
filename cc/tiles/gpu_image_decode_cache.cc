// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/gpu_image_decode_cache.h"

#include <inttypes.h>

#include <algorithm>
#include <limits>
#include <string>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/features.h"
#include "cc/base/histograms.h"
#include "cc/base/switches.h"
#include "cc/paint/paint_flags.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/mipmap_util.h"
#include "cc/tiles/raster_dark_mode_filter.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/skia_span_util.h"
#include "ui/gl/trace_util.h"

namespace cc {

namespace {

// The number or entries to keep in the cache, depending on the memory state of
// the system. This limit can be breached by in-use cache items, which cannot
// be deleted.
static const int kNormalMaxItemsInCacheForGpu = 2000;
static const int kSuspendedMaxItemsInCacheForGpu = 0;

// The maximum number of images that we can lock simultaneously in our working
// set. This is separate from the memory limit, as keeping very large numbers
// of small images simultaneously locked can lead to performance issues and
// memory spikes.
static const int kMaxItemsInWorkingSet = 256;

// lock_count │ used  │ result state
// ═══════════╪═══════╪══════════════════
//  1         │ false │ WASTED_ONCE
//  1         │ true  │ USED_ONCE
//  >1        │ false │ WASTED_RELOCKED
//  >1        │ true  │ USED_RELOCKED
// Note that it's important not to reorder the following enum, since the
// numerical values are used in the histogram code.
enum ImageUsageState : int {
  IMAGE_USAGE_STATE_WASTED_ONCE,
  IMAGE_USAGE_STATE_USED_ONCE,
  IMAGE_USAGE_STATE_WASTED_RELOCKED,
  IMAGE_USAGE_STATE_USED_RELOCKED,
  IMAGE_USAGE_STATE_COUNT
};

// Returns true if an image would not be drawn and should therefore be
// skipped rather than decoded.
bool SkipImage(const DrawImage& draw_image) {
  if (!SkIRect::Intersects(
          draw_image.src_rect(),
          SkIRect::MakeSize(
              draw_image.paint_image().GetSkISize(AuxImage::kDefault)))) {
    return true;
  }
  if (std::abs(draw_image.scale().width()) <
          std::numeric_limits<float>::epsilon() ||
      std::abs(draw_image.scale().height()) <
          std::numeric_limits<float>::epsilon()) {
    return true;
  }
  return false;
}

// Returns the filter quality to use for scaling the image to upload scale as
// well as for using when passing the decoded image to skia. Due to parity with
// SW and power impliciation, limit the filter quality to medium.
PaintFlags::FilterQuality CalculateDesiredFilterQuality(
    const DrawImage& draw_image) {
  return std::min(PaintFlags::FilterQuality::kMedium,
                  draw_image.filter_quality());
}

// Calculates the scale factor which can be used to scale an image to a given
// mip level.
SkSize CalculateScaleFactorForMipLevel(const DrawImage& draw_image,
                                       AuxImage aux_image,
                                       int upload_scale_mip_level) {
  gfx::Size base_size = draw_image.paint_image().GetSize(aux_image);
  return MipMapUtil::GetScaleAdjustmentForLevel(base_size,
                                                upload_scale_mip_level);
}

// Calculates the size of a given mip level.
gfx::Size CalculateSizeForMipLevel(const DrawImage& draw_image,
                                   AuxImage aux_image,
                                   int upload_scale_mip_level) {
  gfx::Size base_size = draw_image.paint_image().GetSize(aux_image);
  return MipMapUtil::GetSizeForLevel(base_size, upload_scale_mip_level);
}

// Determines whether a draw image requires mips.
bool ShouldGenerateMips(const DrawImage& draw_image,
                        AuxImage aux_image,
                        int upload_scale_mip_level) {
  // If filter quality is less than medium, don't generate mips.
  if (draw_image.filter_quality() < PaintFlags::FilterQuality::kMedium)
    return false;

  gfx::Size base_size = draw_image.paint_image().GetSize(aux_image);
  // Take the abs of the scale, as mipmap functions don't handle (and aren't
  // impacted by) negative image dimensions.
  gfx::SizeF scaled_size = gfx::ScaleSize(
      gfx::SizeF(base_size), std::abs(draw_image.scale().width()),
      std::abs(draw_image.scale().height()));

  // If our target size is smaller than our scaled size in both dimension, we
  // need to generate mips.
  gfx::SizeF target_size = gfx::SizeF(
      CalculateSizeForMipLevel(draw_image, aux_image, upload_scale_mip_level));
  if (scaled_size.width() < target_size.width() &&
      scaled_size.height() < target_size.height()) {
    return true;
  }

  return false;
}

// Draws and scales the provided |draw_image| into the |target_pixmap|. If the
// draw/scale can be done directly, calls directly into PaintImage::Decode.
// if not, decodes to a compatible temporary pixmap and then converts that into
// the |target_pixmap|.
bool DrawAndScaleImageRGB(const DrawImage& draw_image,
                          AuxImage aux_image,
                          SkPixmap& target_pixmap,
                          PaintImage::GeneratorClientId client_id) {
  const PaintImage& paint_image = draw_image.paint_image();
  const bool is_original_size_decode =
      paint_image.GetSkISize(aux_image) == target_pixmap.dimensions();
  const bool is_nearest_neighbor =
      draw_image.filter_quality() == PaintFlags::FilterQuality::kNone;

  SkISize supported_size =
      paint_image.GetSupportedDecodeSize(target_pixmap.dimensions(), aux_image);

  // We can directly decode into target pixmap if we are doing an original
  // decode or we are decoding to scale without nearest neighbor filtering.
  const bool can_directly_decode =
      is_original_size_decode || !is_nearest_neighbor;
  if (supported_size == target_pixmap.dimensions() && can_directly_decode) {
    if (!paint_image.Decode(target_pixmap, draw_image.frame_index(), aux_image,
                            client_id)) {
      DLOG(ERROR) << "Failed to decode image.";
      return false;
    }
    return true;
  }

  // If we can't decode/scale directly, we will handle this in 2 steps.
  // Step 1: Decode at the nearest (larger) directly supported size or the
  // original size if nearest neighbor quality is requested.
  const SkISize decode_size =
      is_nearest_neighbor ? paint_image.GetSkISize(aux_image) : supported_size;
  SkImageInfo decode_info = target_pixmap.info().makeDimensions(decode_size);
  SkBitmap decode_bitmap;
  if (!decode_bitmap.tryAllocPixels(decode_info)) {
    DLOG(ERROR) << "Failed to allocate bitmap.";
    return false;
  }
  SkPixmap decode_pixmap = decode_bitmap.pixmap();
  if (!paint_image.Decode(decode_pixmap, draw_image.frame_index(), aux_image,
                          client_id)) {
    DLOG(ERROR) << "Failed to decode unscaled image.";
    return false;
  }

  // Step 2: Scale to |pixmap| size.
  const PaintFlags::FilterQuality filter_quality =
      CalculateDesiredFilterQuality(draw_image);
  const SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(filter_quality));
  if (!decode_pixmap.scalePixels(target_pixmap, sampling)) {
    DLOG(ERROR) << "Failed to scale image.";
    return false;
  }
  return true;
}

// Decode and scale for YUV pixmaps.
//
// The pixmaps in `yuva_pixmaps` share a contiguous block of allocated backing
// memory. If scaling needs to happen, it is done individually for each plane.
bool DrawAndScaleImageYUV(
    const DrawImage& draw_image,
    AuxImage aux_image,
    PaintImage::GeneratorClientId client_id,
    const SkYUVAPixmapInfo::SupportedDataTypes& yuva_supported_data_types,
    SkYUVAPixmaps& yuva_pixmaps) {
  const PaintImage& paint_image = draw_image.paint_image();
  const int num_planes = yuva_pixmaps.numPlanes();

  // Query the decoder's SkYUVAPixmapInfo.
  SkYUVAPixmapInfo decodable_yuva_pixmap_info;
  {
    const bool yuva_info_initialized = paint_image.IsYuv(
        yuva_supported_data_types, aux_image, &decodable_yuva_pixmap_info);
    DCHECK(yuva_info_initialized);
    DCHECK_EQ(decodable_yuva_pixmap_info.dataType(), yuva_pixmaps.dataType());
    DCHECK_EQ(decodable_yuva_pixmap_info.numPlanes(), num_planes);

    // The Y size reported by IsYuv must be a supported decode size.
    SkISize y_target_size =
        decodable_yuva_pixmap_info.planeInfo(0).dimensions();
    SkISize supported_size =
        paint_image.GetSupportedDecodeSize(y_target_size, aux_image);
    DCHECK(y_target_size == supported_size);
  }

  // We can directly decode into target pixmap if we are doing an original size
  // decode.
  // TODO(crbug.com/40612018): Although the JPEG decoder supports decoding to
  // scale, we have not yet implemented YUV + decoding to scale, so we skip it.
  {
    bool is_directly_decodable = true;
    for (int i = 0; i < num_planes; ++i) {
      is_directly_decodable &=
          yuva_pixmaps.plane(i).info().dimensions() ==
          decodable_yuva_pixmap_info.planeInfo(i).dimensions();
    }
    if (is_directly_decodable) {
      if (!paint_image.DecodeYuv(yuva_pixmaps, draw_image.frame_index(),
                                 aux_image, client_id)) {
        DLOG(ERROR) << "Failed to decode image as YUV.";
        return false;
      }
      return true;
    }
  }

  // Allocate `decode_yuva_bytes` in an SkBitmap. This is so that we can use
  // tryAlloc to avoid crashing if allocation fails (having a TryAlloc on
  // SkYUVAPixmaps would be less convolued).
  const size_t decode_yuva_bytes =
      decodable_yuva_pixmap_info.computeTotalBytes();
  if (SkImageInfo::ByteSizeOverflowed(decode_yuva_bytes)) {
    DLOG(ERROR) << "YUVA image size overflowed.";
    return false;
  }
  SkBitmap decode_buffer_bitmap;
  if (!decode_buffer_bitmap.tryAllocPixels(SkImageInfo::Make(
          decode_yuva_bytes, 1, kR8_unorm_SkColorType, kOpaque_SkAlphaType))) {
    DLOG(ERROR) << "Failed to allocate decode YUV storage.";
    return false;
  }

  // Decode at the original size.
  SkYUVAPixmaps decode_yuva_pixmaps = SkYUVAPixmaps::FromExternalMemory(
      decodable_yuva_pixmap_info, decode_buffer_bitmap.getPixels());
  if (!paint_image.DecodeYuv(decode_yuva_pixmaps, draw_image.frame_index(),
                             aux_image, client_id)) {
    DLOG(ERROR) << "Failed to decode decode image as YUV.";
    return false;
  }

  // Scale to the target size, plane-by-plane.
  const PaintFlags::FilterQuality filter_quality =
      CalculateDesiredFilterQuality(draw_image);
  const SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(filter_quality));
  for (int i = 0; i < num_planes; ++i) {
    const SkPixmap& decode = decode_yuva_pixmaps.plane(i);
    const SkPixmap& scaled = yuva_pixmaps.plane(i);
    if (!decode.scalePixels(scaled, sampling)) {
      DLOG(ERROR) << "Failed to scale YUV planes.";
      return false;
    }
  }
  return true;
}

// We use this below, instead of just a std::unique_ptr, so that we can run
// a Finch experiment to check the impact of not using discardable memory on the
// GPU decode path.
class HeapDiscardableMemory : public base::DiscardableMemory {
 public:
  explicit HeapDiscardableMemory(size_t size)
      : memory_(base::HeapArray<char>::Uninit(size)), size_(size) {}
  ~HeapDiscardableMemory() override = default;
  [[nodiscard]] bool Lock() override {
    // Locking only succeeds when we have not yet discarded the memory (i.e. if
    // we have never called |Unlock()|.)
    return !memory_.empty();
  }
  void Unlock() override { Discard(); }
  void* data() const override {
    DCHECK(!memory_.empty());
    return const_cast<char*>(memory_.data());
  }
  void DiscardForTesting() override { Discard(); }
  base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
      const char* name,
      base::trace_event::ProcessMemoryDump* pmd) const override {
    auto* dump = pmd->CreateAllocatorDump(name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes, size_);
    return dump;
  }

 private:
  void Discard() {
    memory_ = base::HeapArray<char>();
    size_ = 0;
  }

  base::HeapArray<char> memory_;
  size_t size_;
};

std::optional<SkYUVAPixmapInfo> GetYUVADecodeInfo(
    const DrawImage& draw_image,
    AuxImage aux_image,
    const SkISize target_size,
    const SkYUVAPixmapInfo::SupportedDataTypes& yuva_supported_data_types) {
  SkYUVAPixmapInfo original_yuva_pixmap_info;
  if (!draw_image.paint_image().IsYuv(yuva_supported_data_types, aux_image,
                                      &original_yuva_pixmap_info)) {
    return std::nullopt;
  }
  DCHECK(original_yuva_pixmap_info.isValid());

  if (target_size != original_yuva_pixmap_info.yuvaInfo().dimensions()) {
    // Always promote scaled images to 4:4:4 to avoid blurriness. By using the
    // same dimensions for the UV planes, we can avoid scaling them completely
    // or at least avoid scaling the width.
    //
    // E.g., consider an original (100, 100) image scaled to mips level 1 (50%),
    // the Y plane size will be (50, 50), but unscaled UV planes are already
    // (50, 50) for 4:2:0, and (50, 100) for 4:2:2, so leaving them completely
    // unscaled or only scaling the height for 4:2:2 has superior quality.
    SkYUVAInfo scaled_yuva_info =
        original_yuva_pixmap_info.yuvaInfo()
            .makeSubsampling(SkYUVAInfo::Subsampling::k444)
            .makeDimensions(target_size);
    return SkYUVAPixmapInfo(scaled_yuva_info,
                            original_yuva_pixmap_info.dataType(), nullptr);
  }
  // Original size decode.
  return original_yuva_pixmap_info;
}

}  // namespace

// Extract the information to uniquely identify a DrawImage for the purposes of
// the |in_use_cache_|.
GpuImageDecodeCache::InUseCacheKey::InUseCacheKey(const DrawImage& draw_image,
                                                  int mip_level)
    : frame_key(draw_image.frame_key()),
      upload_scale_mip_level(mip_level),
      filter_quality(CalculateDesiredFilterQuality(draw_image)),
      target_color_space(draw_image.target_color_space()) {}

bool GpuImageDecodeCache::InUseCacheKey::operator==(
    const InUseCacheKey& other) const {
  return frame_key == other.frame_key &&
         upload_scale_mip_level == other.upload_scale_mip_level &&
         filter_quality == other.filter_quality &&
         target_color_space == other.target_color_space;
}

size_t GpuImageDecodeCache::InUseCacheKeyHash::operator()(
    const InUseCacheKey& cache_key) const {
  return base::HashInts(
      cache_key.target_color_space.GetHash(),
      base::HashInts(
          cache_key.frame_key.hash(),
          base::HashInts(cache_key.mip_level(),
                         static_cast<int>(cache_key.filter_quality))));
}

GpuImageDecodeCache::InUseCacheEntry::InUseCacheEntry(
    scoped_refptr<ImageData> image_data)
    : image_data(std::move(image_data)) {}
GpuImageDecodeCache::InUseCacheEntry::InUseCacheEntry(const InUseCacheEntry&) =
    default;
GpuImageDecodeCache::InUseCacheEntry::InUseCacheEntry(InUseCacheEntry&&) =
    default;
GpuImageDecodeCache::InUseCacheEntry::~InUseCacheEntry() = default;

// Task which decodes an image and stores the result in discardable memory.
// This task does not use GPU resources and can be run on any thread.
class GpuImageDecodeTaskImpl : public TileTask {
 public:
  GpuImageDecodeTaskImpl(GpuImageDecodeCache* cache,
                         const DrawImage& draw_image,
                         const ImageDecodeCache::TracingInfo& tracing_info,
                         ImageDecodeCache::TaskType task_type,
                         ImageDecodeCache::ClientId client_id)
      : TileTask(TileTask::SupportsConcurrentExecution::kYes,
                 TileTask::SupportsBackgroundThreadPriority::kNo),
        cache_(cache),
        image_(draw_image),
        tracing_info_(tracing_info),
        task_type_(task_type),
        client_id_(client_id) {
    DCHECK(!SkipImage(draw_image));
  }
  GpuImageDecodeTaskImpl(const GpuImageDecodeTaskImpl&) = delete;

  GpuImageDecodeTaskImpl& operator=(const GpuImageDecodeTaskImpl&) = delete;

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "GpuImageDecodeTaskImpl::RunOnWorkerThread", "mode",
                 "gpu", "source_prepare_tiles_id",
                 tracing_info_.prepare_tiles_id);

    const auto* image_metadata = image_.paint_image().GetImageHeaderMetadata();
    const ImageType image_type =
        image_metadata ? image_metadata->image_type : ImageType::kInvalid;
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        &image_.paint_image(),
        devtools_instrumentation::ScopedImageDecodeTask::DecodeType::kGpu,
        ImageDecodeCache::ToScopedTaskType(task_type_),
        ImageDecodeCache::ToScopedImageType(image_type));
    cache_->DecodeImageInTask(image_, task_type_);
  }

  // Overridden from TileTask:
  bool IsRasterTask() const override {
    return task_type_ == ImageDecodeCache::TaskType::kInRaster;
  }
  void OnTaskCompleted() override {
    cache_->OnImageDecodeTaskCompleted(image_, task_type_, client_id_);
  }

  // Overridden from TileTask:
  bool TaskContainsLCPCandidateImages() const override {
    if (!HasCompleted() && image_.paint_image().may_be_lcp_candidate())
      return true;
    return TileTask::TaskContainsLCPCandidateImages();
  }

 protected:
  ~GpuImageDecodeTaskImpl() override = default;

 private:
  raw_ptr<GpuImageDecodeCache> cache_;
  DrawImage image_;
  const ImageDecodeCache::TracingInfo tracing_info_;
  const ImageDecodeCache::TaskType task_type_;
  const ImageDecodeCache::ClientId client_id_;
};

// Task which creates an image from decoded data. Typically this involves
// uploading data to the GPU, which requires this task be run on the non-
// concurrent thread.
class ImageUploadTaskImpl : public TileTask {
 public:
  ImageUploadTaskImpl(GpuImageDecodeCache* cache,
                      const DrawImage& draw_image,
                      scoped_refptr<TileTask> decode_dependency,
                      const ImageDecodeCache::TracingInfo& tracing_info,
                      ImageDecodeCache::ClientId client_id)
      : TileTask(TileTask::SupportsConcurrentExecution::kNo,
                 TileTask::SupportsBackgroundThreadPriority::kYes),
        cache_(cache),
        image_(draw_image),
        tracing_info_(tracing_info),
        client_id_(client_id) {
    DCHECK(!SkipImage(draw_image));
    // If an image is already decoded and locked, we will not generate a
    // decode task.
    if (decode_dependency)
      dependencies_.push_back(std::move(decode_dependency));
  }
  ImageUploadTaskImpl(const ImageUploadTaskImpl&) = delete;

  ImageUploadTaskImpl& operator=(const ImageUploadTaskImpl&) = delete;

  // Override from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageUploadTaskImpl::RunOnWorkerThread", "mode", "gpu",
                 "source_prepare_tiles_id", tracing_info_.prepare_tiles_id);
    const auto* image_metadata = image_.paint_image().GetImageHeaderMetadata();
    const ImageType image_type =
        image_metadata ? image_metadata->image_type : ImageType::kInvalid;
    devtools_instrumentation::ScopedImageUploadTask image_upload_task(
        &image_.paint_image(), ImageDecodeCache::ToScopedImageType(image_type));
    cache_->UploadImageInTask(image_);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    cache_->OnImageUploadTaskCompleted(image_, client_id_);
  }

 protected:
  ~ImageUploadTaskImpl() override = default;

 private:
  raw_ptr<GpuImageDecodeCache> cache_;
  DrawImage image_;
  const ImageDecodeCache::TracingInfo tracing_info_;
  const ImageDecodeCache::ClientId client_id_;
};

////////////////////////////////////////////////////////////////////////////////
// GpuImageDecodeCache::ImageDataBase

GpuImageDecodeCache::ImageDataBase::ImageDataBase() = default;
GpuImageDecodeCache::ImageDataBase::~ImageDataBase() = default;

void GpuImageDecodeCache::ImageDataBase::OnSetLockedData(bool out_of_raster) {
  DCHECK_EQ(usage_stats_.lock_count, 1);
  DCHECK(!is_locked_);
  usage_stats_.first_lock_out_of_raster = out_of_raster;
  is_locked_ = true;
}

void GpuImageDecodeCache::ImageDataBase::OnResetData() {
  is_locked_ = false;
  usage_stats_ = UsageStats();
}

void GpuImageDecodeCache::ImageDataBase::OnLock() {
  DCHECK(!is_locked_);
  is_locked_ = true;
  ++usage_stats_.lock_count;
}

void GpuImageDecodeCache::ImageDataBase::OnUnlock() {
  DCHECK(is_locked_);
  is_locked_ = false;
  if (usage_stats_.lock_count == 1)
    usage_stats_.first_lock_wasted = !usage_stats_.used;
}

int GpuImageDecodeCache::ImageDataBase::UsageState() const {
  ImageUsageState state = IMAGE_USAGE_STATE_WASTED_ONCE;
  if (usage_stats_.lock_count == 1) {
    if (usage_stats_.used)
      state = IMAGE_USAGE_STATE_USED_ONCE;
    else
      state = IMAGE_USAGE_STATE_WASTED_ONCE;
  } else {
    if (usage_stats_.used)
      state = IMAGE_USAGE_STATE_USED_RELOCKED;
    else
      state = IMAGE_USAGE_STATE_WASTED_RELOCKED;
  }

  return state;
}

////////////////////////////////////////////////////////////////////////////////
// GpuImageDecodeCache::DecodedAuxImageData

GpuImageDecodeCache::DecodedAuxImageData::DecodedAuxImageData() = default;

GpuImageDecodeCache::DecodedAuxImageData::DecodedAuxImageData(
    const SkPixmap& rgba_pixmap,
    std::unique_ptr<base::DiscardableMemory> in_data) {
  data = std::move(in_data);
  auto release_proc = [](const void*, void*) {};
  images[0] = SkImages::RasterFromPixmap(rgba_pixmap, release_proc, nullptr);
  pixmaps[0] = rgba_pixmap;
  ValidateImagesMatchPixmaps();
}

GpuImageDecodeCache::DecodedAuxImageData::DecodedAuxImageData(
    const SkYUVAPixmaps& yuva_pixmaps,
    std::unique_ptr<base::DiscardableMemory> in_data) {
  data = std::move(in_data);
  auto release_proc = [](const void*, void*) {};
  for (int plane = 0; plane < yuva_pixmaps.numPlanes(); ++plane) {
    images[plane] = SkImages::RasterFromPixmap(yuva_pixmaps.plane(plane),
                                               release_proc, nullptr);
    pixmaps[plane] = yuva_pixmaps.plane(plane);
  }
  ValidateImagesMatchPixmaps();
}

GpuImageDecodeCache::DecodedAuxImageData::DecodedAuxImageData(
    DecodedAuxImageData&& other)
    : data(std::move(other.data)) {
  for (int plane = 0; plane < SkYUVAInfo::kMaxPlanes; ++plane) {
    images[plane] = std::move(other.images[plane]);
    pixmaps[plane] = other.pixmaps[plane];
  }
  ValidateImagesMatchPixmaps();
  other.ResetData();
}

GpuImageDecodeCache::DecodedAuxImageData&
GpuImageDecodeCache::DecodedAuxImageData::operator=(
    DecodedAuxImageData&& other) {
  data = std::move(other.data);
  other.data = nullptr;
  for (int plane = 0; plane < SkYUVAInfo::kMaxPlanes; ++plane) {
    images[plane] = std::move(other.images[plane]);
    pixmaps[plane] = other.pixmaps[plane];
    other.images[plane] = nullptr;
    other.pixmaps[plane] = SkPixmap();
  }
  ValidateImagesMatchPixmaps();
  return *this;
}

GpuImageDecodeCache::DecodedAuxImageData::~DecodedAuxImageData() = default;

bool GpuImageDecodeCache::DecodedAuxImageData::IsEmpty() const {
  ValidateImagesMatchPixmaps();

  // If `data` is present, then there must be at least one image and pixmap.
  if (data) {
    DCHECK(images[0]);
    return false;
  }
  // A bitmap-backed DecodedAuxImageData will have an `images` and `pixmaps`,
  // but no data.
  if (images[0]) {
    for (int i = 1; i < SkYUVAInfo::kMaxPlanes; ++i) {
      DCHECK(!images[i]);
    }
    return false;
  }
  return true;
}

void GpuImageDecodeCache::DecodedAuxImageData::ResetData() {
  ValidateImagesMatchPixmaps();
  data = nullptr;
  for (auto& image : images) {
    image = nullptr;
  }
  for (auto& pixmap : pixmaps) {
    pixmap = SkPixmap();
  }
  ValidateImagesMatchPixmaps();
  DCHECK(IsEmpty());
}

////////////////////////////////////////////////////////////////////////////////
// GpuImageDecodeCache::DecodedImageData

GpuImageDecodeCache::DecodedImageData::DecodedImageData(bool is_bitmap_backed)
    : is_bitmap_backed_(is_bitmap_backed) {
  for (const auto& aux_image_data : aux_image_data_) {
    aux_image_data.ValidateImagesMatchPixmaps();
  }
}

GpuImageDecodeCache::DecodedImageData::~DecodedImageData() {
  for (const auto& aux_image_data : aux_image_data_) {
    aux_image_data.ValidateImagesMatchPixmaps();
  }
  ResetData();
}

bool GpuImageDecodeCache::DecodedImageData::Lock() {
  DCHECK(!is_bitmap_backed_);
  for (const auto& aux_image_data : aux_image_data_) {
    aux_image_data.ValidateImagesMatchPixmaps();
  }

  bool did_lock = true;
  std::array<bool, kAuxImageCount> did_lock_image = {false, false};
  for (size_t i = 0; i < kAuxImageCount; ++i) {
    if (!aux_image_data_[i].data) {
      continue;
    }
    did_lock_image[i] = aux_image_data_[i].data->Lock();
    if (did_lock_image[i]) {
      continue;
    }

    // If we fail to lock an image, unlock all images that we locked in this
    // loop, and break out of the loop.
    for (size_t j = 0; j < i; ++j) {
      if (did_lock_image[j]) {
        aux_image_data_[j].data->Unlock();
      }
    }
    did_lock = false;
    break;
  }
  if (did_lock) {
    OnLock();
  }
  return is_locked_;
}

void GpuImageDecodeCache::DecodedImageData::Unlock() {
  for (auto& aux_image_data : aux_image_data_) {
    if (aux_image_data.data) {
      aux_image_data.data->Unlock();
    }
  }
  OnUnlock();
}

void GpuImageDecodeCache::DecodedImageData::SetLockedData(
    base::span<DecodedAuxImageData, kAuxImageCount> aux_image_data,
    bool out_of_raster) {
  for (size_t i = 0; i < kAuxImageCount; ++i) {
    DCHECK(aux_image_data_[i].IsEmpty());
    aux_image_data[i].ValidateImagesMatchPixmaps();
    aux_image_data_[i] = std::move(aux_image_data[i]);
  }

  // A default image must have been set.
  DCHECK(!aux_image_data_[kAuxImageIndexDefault].IsEmpty());
  for (size_t i = 0; i < kAuxImageCount; ++i) {
    aux_image_data_[i].ValidateImagesMatchPixmaps();
  }
  OnSetLockedData(out_of_raster);
}

void GpuImageDecodeCache::DecodedImageData::SetBitmapImage(
    sk_sp<SkImage> image) {
  DCHECK(is_bitmap_backed_);
  for (const auto& aux_image_data : aux_image_data_) {
    DCHECK(aux_image_data.IsEmpty());
  }
  aux_image_data_[kAuxImageIndexDefault].images[0] = std::move(image);
  aux_image_data_[kAuxImageIndexDefault].images[0]->peekPixels(
      &aux_image_data_[kAuxImageIndexDefault].pixmaps[0]);
  aux_image_data_[kAuxImageIndexDefault].ValidateImagesMatchPixmaps();

  for (const auto& aux_image_data : aux_image_data_) {
    aux_image_data.ValidateImagesMatchPixmaps();
  }
  OnLock();
}

void GpuImageDecodeCache::DecodedImageData::ResetBitmapImage() {
  DCHECK(is_bitmap_backed_);
  // Bitmaps only ever have a single SkImage.
  aux_image_data_[0].ResetData();
  for (auto& aux_image_data : aux_image_data_) {
    DCHECK(aux_image_data.IsEmpty());
  }
  OnUnlock();
}

void GpuImageDecodeCache::ImageData::RecordSpeculativeDecodeMatch(
    int mip_level) {
  if (speculative_decode_usage_stats_.has_value()) {
    speculative_decode_usage_stats_->min_raster_mip_level = std::min(
        speculative_decode_usage_stats_->min_raster_mip_level, mip_level);
  }
}

void GpuImageDecodeCache::ImageData::
    RecordSpeculativeDecodeRasterTaskTakeover() {
  if (speculative_decode_usage_stats_.has_value()) {
    speculative_decode_usage_stats_->raster_task_takeover = true;
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("loading"),
                         "SpeculativeImageDecodeRasterTaskTakeover",
                         TRACE_EVENT_SCOPE_THREAD, "image_id", paint_image_id);
  }
}

void GpuImageDecodeCache::DecodedImageData::ResetData() {
  if (aux_image_data_[kAuxImageIndexDefault].data) {
    ReportUsageStats();
  }
  for (auto& aux_image_data : aux_image_data_) {
    aux_image_data.ResetData();
  }
  OnResetData();
}

void GpuImageDecodeCache::DecodedImageData::ReportUsageStats() const {
  UMA_HISTOGRAM_ENUMERATION("Renderer4.GpuImageDecodeState",
                            static_cast<ImageUsageState>(UsageState()),
                            IMAGE_USAGE_STATE_COUNT);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuImageDecodeState.FirstLockWasted",
                        usage_stats_.first_lock_wasted);
  if (usage_stats_.first_lock_out_of_raster)
    UMA_HISTOGRAM_BOOLEAN(
        "Renderer4.GpuImageDecodeState.FirstLockWasted.OutOfRaster",
        usage_stats_.first_lock_wasted);
}

////////////////////////////////////////////////////////////////////////////////
// GpuImageDecodeCache::UploadedImageData

GpuImageDecodeCache::UploadedImageData::UploadedImageData() = default;
GpuImageDecodeCache::UploadedImageData::~UploadedImageData() = default;

void GpuImageDecodeCache::UploadedImageData::SetTransferCacheId(uint32_t id) {
  DCHECK(!transfer_cache_id_);

  transfer_cache_id_ = id;
  OnSetLockedData(false /* out_of_raster */);
}

void GpuImageDecodeCache::UploadedImageData::Reset() {
  if (transfer_cache_id_) {
    ReportUsageStats();
  }
  transfer_cache_id_.reset();
  OnResetData();
}

void GpuImageDecodeCache::UploadedImageData::ReportUsageStats() const {
  UMA_HISTOGRAM_ENUMERATION("Renderer4.GpuImageUploadState",
                            static_cast<ImageUsageState>(UsageState()),
                            IMAGE_USAGE_STATE_COUNT);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuImageUploadState.FirstLockWasted",
                        usage_stats_.first_lock_wasted);
}

////////////////////////////////////////////////////////////////////////////////
// GpuImageDecodeCache::ImageInfo

GpuImageDecodeCache::ImageInfo::ImageInfo() = default;

GpuImageDecodeCache::ImageInfo::ImageInfo(const SkImageInfo& rgba)
    : rgba(rgba), size(rgba.computeMinByteSize()) {
  DCHECK(!SkImageInfo::ByteSizeOverflowed(size));
}

GpuImageDecodeCache::ImageInfo::ImageInfo(const SkYUVAPixmapInfo& yuva)
    : yuva(yuva), size(yuva.computeTotalBytes()) {
  DCHECK(!SkImageInfo::ByteSizeOverflowed(size));
}

GpuImageDecodeCache::ImageInfo::ImageInfo(const ImageInfo&) = default;

GpuImageDecodeCache::ImageInfo& GpuImageDecodeCache::ImageInfo::operator=(
    const ImageInfo&) = default;

GpuImageDecodeCache::ImageInfo::~ImageInfo() = default;

////////////////////////////////////////////////////////////////////////////////
// GpuImageDecodeCache::ImageData

GpuImageDecodeCache::ImageData::ImageData(
    PaintImage::Id paint_image_id_param,
    const gfx::ColorSpace& target_color_space,
    PaintFlags::FilterQuality quality,
    int upload_scale_mip_level_param,
    bool needs_mips,
    bool is_bitmap_backed,
    bool speculative_decode,
    base::span<ImageInfo, kAuxImageCount> image_info)
    : paint_image_id(paint_image_id_param),
      target_color_space(target_color_space),
      quality(quality),
      upload_scale_mip_level(upload_scale_mip_level_param),
      needs_mips(needs_mips),
      is_bitmap_backed(is_bitmap_backed),
      info(std::move(image_info[kAuxImageIndexDefault])),
      gainmap_info(std::move(image_info[kAuxImageIndexGainmap])),
      decode(is_bitmap_backed) {
  if (info.yuva.has_value()) {
    // This is the only plane config supported by non-OOP raster.
    DCHECK_EQ(info.yuva->yuvaInfo().planeConfig(),
              SkYUVAInfo::PlaneConfig::kY_U_V);
  }
  if (base::FeatureList::IsEnabled(features::kInitImageDecodeLastUseTime)) {
    last_use = base::TimeTicks::Now();
  }
  if (speculative_decode) {
    speculative_decode_usage_stats_.emplace();
    speculative_decode_usage_stats_->speculative_decode_mip_level =
        upload_scale_mip_level;
    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("loading"),
                         "SpeculativeImageDecodeTaskCreated",
                         TRACE_EVENT_SCOPE_THREAD, "image_id", paint_image_id,
                         "speculative_mip_level", upload_scale_mip_level);
  }
}

GpuImageDecodeCache::ImageData::~ImageData() {
  // We should never delete ImageData while it is in use or before it has been
  // cleaned up.
  DCHECK_EQ(0u, upload.ref_count);
  DCHECK_EQ(0u, decode.ref_count);
  DCHECK_EQ(false, decode.is_locked());
  // This should always be cleaned up before deleting the image, as it needs to
  // be freed with the GL context lock held.
  DCHECK(!HasUploadedData());
  if (IsSpeculativeDecode() &&
      speculative_decode_usage_stats_->min_raster_mip_level == INT_MAX) {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("loading"),
                         "SpeculativeImageDecodeUnused",
                         TRACE_EVENT_SCOPE_THREAD, "image_id", paint_image_id);
  }
  speculative_decode_usage_stats_.reset();
}

bool GpuImageDecodeCache::ImageData::HasUploadedData() const {
  return !!upload.transfer_cache_id();
}

void GpuImageDecodeCache::ImageData::ValidateBudgeted() const {
  // If the image is budgeted, it must be refed.
  DCHECK(is_budgeted);
  DCHECK_GT(upload.ref_count, 0u);
}

size_t GpuImageDecodeCache::ImageData::GetTotalSize() const {
  size_t size = 0;
  for (const auto aux_image : kAllAuxImages) {
    const auto& aux_image_info = GetImageInfo(aux_image);
    size += aux_image_info.size;
  }
  return size;
}

////////////////////////////////////////////////////////////////////////////////
// GpuImageDecodeCache

GpuImageDecodeCache::GpuImageDecodeCache(
    viz::RasterContextProvider* context,
    SkColorType color_type,
    size_t max_working_set_bytes,
    int max_texture_size,
    RasterDarkModeFilter* const dark_mode_filter)
    : color_type_(color_type),
      context_(context),
      max_texture_size_(max_texture_size),
      generator_client_id_(PaintImage::GetNextGeneratorClientId()),
      enable_clipped_image_scaling_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableClippedImageScaling)),
      persistent_cache_(PersistentCache::NO_AUTO_EVICT),
      max_working_set_bytes_(max_working_set_bytes),
      max_working_set_items_(kMaxItemsInWorkingSet),
      dark_mode_filter_(dark_mode_filter) {
  if (base::SequencedTaskRunner::HasCurrentDefault()) {
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }

  DCHECK_NE(generator_client_id_, PaintImage::kDefaultGeneratorClientId);

  {
    // TODO(crbug.com/40141944): We shouldn't need to lock to get capabilities.
    std::optional<viz::RasterContextProvider::ScopedRasterContextLock>
        context_lock;
    if (context_->GetLock())
      context_lock.emplace(context_);
    const auto& caps = context_->ContextCapabilities();
    yuva_supported_data_types_.enableDataType(
        SkYUVAPixmapInfo::DataType::kUnorm8, 1);
    if (caps.texture_norm16) {
      yuva_supported_data_types_.enableDataType(
          SkYUVAPixmapInfo::DataType::kUnorm16, 1);
    }
    if (caps.texture_half_float_linear) {
      yuva_supported_data_types_.enableDataType(
          SkYUVAPixmapInfo::DataType::kFloat16, 1);
    }
  }

  // In certain cases, SingleThreadTaskRunner::CurrentDefaultHandle isn't set
  // (Android Webview).  Don't register a dump provider in these cases.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::GpuImageDecodeCache",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
  memory_pressure_listener_registration_ =
      std::make_unique<base::AsyncMemoryPressureListenerRegistration>(
          FROM_HERE, base::MemoryPressureListenerTag::kGpuImageDecodeCache,
          this);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::DarkModeFilter", "dark_mode_filter",
               static_cast<void*>(dark_mode_filter_));
}

GpuImageDecodeCache::~GpuImageDecodeCache() {
  // Debugging crbug.com/650234.
  CHECK_EQ(0u, in_use_cache_.size());

  // SetShouldAggressivelyFreeResources will zero our limits and free all
  // outstanding image memory.
  SetShouldAggressivelyFreeResources(true);

  // It is safe to unregister, even if we didn't register in the constructor.
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

ImageDecodeCache::TaskResult GpuImageDecodeCache::GetTaskForImageAndRef(
    ClientId client_id,
    const DrawImage& draw_image,
    const TracingInfo& tracing_info) {
  return GetTaskForImageAndRefInternal(client_id, draw_image, tracing_info,
                                       TaskType::kInRaster,
                                       /*speculative*/ false);
}

ImageDecodeCache::TaskResult
GpuImageDecodeCache::GetOutOfRasterDecodeTaskForImageAndRef(
    ClientId client_id,
    const DrawImage& draw_image,
    bool speculative) {
  return GetTaskForImageAndRefInternal(client_id, draw_image,
                                       TracingInfo(0, TilePriority::NOW),
                                       TaskType::kOutOfRaster, speculative);
}

ImageDecodeCache::TaskResult GpuImageDecodeCache::GetTaskForImageAndRefInternal(
    ClientId client_id,
    const DrawImage& draw_image,
    const TracingInfo& tracing_info,
    TaskType task_type,
    bool speculative) {
  DCHECK_GE(client_id, kDefaultClientId);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetTaskForImageAndRef", "client_id",
               client_id);

  if (SkipImage(draw_image)) {
    return TaskResult(false /* need_unref */, false /* is_at_raster_decode */);
  }

  base::AutoLock locker(lock_);
  const InUseCacheKey cache_key = InUseCacheKeyFromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(
      draw_image, cache_key, task_type == TaskType::kInRaster);
  scoped_refptr<ImageData> new_data;
  if (!image_data) {
    new_data = CreateImageData(
        draw_image,
        speculative);
    image_data = new_data.get();
  } else if (image_data->decode.decode_failure) {
    // We have already tried and failed to decode this image, so just return.
    return TaskResult(false /* need_unref */, false /* is_at_raster_decode */);
  } else if (task_type == TaskType::kInRaster &&
             !image_data->upload.task_map.empty() &&
             !image_data->HasUploadedData()) {
    // If there are pending upload tasks and we haven't had data uploaded yet,
    // another task can be created.

    // We had an existing upload task, ref the image and return the task.
    image_data->ValidateBudgeted();
    RefImage(draw_image, cache_key);

    // If we had a task for the same image and the |client_id|, refptr will be
    // returned. Otherwise, create a new task for a new client and the same
    // image and return it.
    scoped_refptr<TileTask> task =
        GetTaskFromMapForClientId(client_id, image_data->upload.task_map);
    if (!task) {
      // Given it's a new task for this |client_id|, the image must be reffed
      // before creating a task - this ref is owned by the caller, and it is
      // their responsibility to release it by calling UnrefImage.
      RefImage(draw_image, cache_key);

      task = base::MakeRefCounted<ImageUploadTaskImpl>(
          this, draw_image,
          GetImageDecodeTaskAndRef(client_id, draw_image, tracing_info,
                                   task_type),
          tracing_info, client_id);
      image_data->upload.task_map[client_id] = task;
    }
    DCHECK(task);
    return TaskResult(task);
  } else if (task_type == TaskType::kOutOfRaster &&
             !image_data->decode.stand_alone_task_map.empty() &&
             !image_data->HasUploadedData()) {
    // If there are pending decode tasks and we haven't had decoded data yet,
    // another task can be created.

    // We had an existing out of raster task, ref the image and return the task.
    image_data->ValidateBudgeted();
    RefImage(draw_image, cache_key);

    // If we had a task for the same image and the |client_id|, refptr will be
    // returned. Otherwise, create a new task for a new client and the same
    // image and return it.
    scoped_refptr<TileTask> task = GetTaskFromMapForClientId(
        client_id, image_data->decode.stand_alone_task_map);
    if (!task) {
      // Even though it's a new task for this client, we don't need to have
      // additional reference here (which the caller is responsible for) as
      // GetImageDecodeTaskAndRef does that for us.

      task = GetImageDecodeTaskAndRef(client_id, draw_image, tracing_info,
                                      task_type);
#if DCHECK_IS_ON()
      scoped_refptr<TileTask> found_task = GetTaskFromMapForClientId(
          client_id, image_data->decode.stand_alone_task_map);
      CHECK_EQ(task, found_task);
#endif
    }

    // This will be null if the image was already decoded.
    if (task)
      return TaskResult(task);
    return TaskResult(/*need_unref=*/true, /*is_at_raster_decode=*/false);
  }

  // Ensure that the image we're about to decode/upload will fit in memory, if
  // not already budgeted.
  if (!image_data->is_budgeted && !EnsureCapacity(image_data->GetTotalSize())) {
    // Image will not fit, do an at-raster decode.
    return TaskResult(false /* need_unref */, true /* is_at_raster_decode */);
  }

  // If we had to create new image data, add it to our map now that we know it
  // will fit.
  if (new_data)
    AddToPersistentCache(draw_image, std::move(new_data));

  // Ref the image before creating a task - this ref is owned by the caller, and
  // it is their responsibility to release it by calling UnrefImage.
  RefImage(draw_image, cache_key);

  // If we already have an image and it is locked (or lock-able), just return
  // that. The image must be budgeted before we attempt to lock it.
  DCHECK(image_data->is_budgeted);
  if (image_data->HasUploadedData() &&
      TryLockImage(HaveContextLock::kNo, draw_image, image_data)) {
    return TaskResult(true /* need_unref */, false /* is_at_raster_decode */);
  }

  scoped_refptr<TileTask> task;
  if (task_type == TaskType::kInRaster) {
    // Ref image and create a upload and decode tasks. We will release this ref
    // in UploadTaskCompleted.
    RefImage(draw_image, cache_key);
    task = base::MakeRefCounted<ImageUploadTaskImpl>(
        this, draw_image,
        GetImageDecodeTaskAndRef(client_id, draw_image, tracing_info,
                                 task_type),
        tracing_info, client_id);
    image_data->upload.task_map[client_id] = task;
  } else {
    task = GetImageDecodeTaskAndRef(client_id, draw_image, tracing_info,
                                    task_type);
  }

  if (task) {
    return TaskResult(task);
  }

  return TaskResult(true /* needs_unref */, false /* is_at_raster_decode */);
}

void GpuImageDecodeCache::UnrefImage(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UnrefImage");
  base::AutoLock lock(lock_);
  UnrefImageInternal(draw_image, InUseCacheKeyFromDrawImage(draw_image));
}

bool GpuImageDecodeCache::UseCacheForDrawImage(
    const DrawImage& draw_image) const {
  if (draw_image.paint_image().IsTextureBacked())
    return false;

  return true;
}

DecodedDrawImage GpuImageDecodeCache::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetDecodedImageForDraw");

  // We are being called during raster. The context lock must already be
  // acquired by the caller.
  CheckContextLockAcquiredIfNecessary();

  // If we're skipping the image, then the filter quality doesn't matter.
  if (SkipImage(draw_image))
    return DecodedDrawImage();

  base::AutoLock lock(lock_);
  const InUseCacheKey cache_key = InUseCacheKeyFromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key, true);
  if (!image_data) {
    // We didn't find the image, create a new entry.
    auto data = CreateImageData(draw_image, false /* speculative_decode */);
    image_data = data.get();
    AddToPersistentCache(draw_image, std::move(data));
  }

  // Ref the image and decode so that they stay alive while we are
  // decoding/uploading.
  // Note that refing the image will attempt to budget the image, if not already
  // done.
  RefImage(draw_image, cache_key);
  RefImageDecode(draw_image, cache_key);

  // We may or may not need to decode and upload the image we've found, the
  // following functions early-out to if we already decoded.
  DecodeImageAndGenerateDarkModeFilterIfNecessary(draw_image, image_data,
                                                  TaskType::kInRaster);
  UploadImageIfNecessary(draw_image, image_data);
  // Unref the image decode, but not the image. The image ref will be released
  // in DrawWithImageFinished.
  UnrefImageDecode(draw_image, cache_key);

  sk_sp<ColorFilter> dark_mode_color_filter = nullptr;
  if (draw_image.use_dark_mode()) {
    auto it = image_data->decode.dark_mode_color_filter_cache.find(
        draw_image.src_rect());
    if (it != image_data->decode.dark_mode_color_filter_cache.end())
      dark_mode_color_filter = it->second;
  }

  auto id = image_data->upload.transfer_cache_id();
  if (id) {
    image_data->upload.mark_used();
  }
  DCHECK(id || image_data->decode.decode_failure);

  SkSize scale_factor = CalculateScaleFactorForMipLevel(
      draw_image, AuxImage::kDefault, image_data->upload_scale_mip_level);
  DecodedDrawImage decoded_draw_image(
      id, std::move(dark_mode_color_filter), SkSize(), scale_factor,
      CalculateDesiredFilterQuality(draw_image), image_data->needs_mips,
      image_data->is_budgeted);
  return decoded_draw_image;
}

void GpuImageDecodeCache::DrawWithImageFinished(
    const DrawImage& draw_image,
    const DecodedDrawImage& decoded_draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::DrawWithImageFinished");

  // Release decoded_draw_image to ensure the referenced SkImage can be
  // cleaned up below.
  { auto delete_decoded_draw_image = std::move(decoded_draw_image); }

  // We are being called during raster. The context lock must already be
  // acquired by the caller.
  CheckContextLockAcquiredIfNecessary();

  if (SkipImage(draw_image))
    return;

  base::AutoLock lock(lock_);
  UnrefImageInternal(draw_image, InUseCacheKeyFromDrawImage(draw_image));

  // We are mid-draw and holding the context lock, ensure we clean up any
  // textures (especially at-raster), which may have just been marked for
  // deletion by UnrefImage.
  RunPendingContextThreadOperations();
}

void GpuImageDecodeCache::ReduceCacheUsage() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::ReduceCacheUsage");
  base::AutoLock lock(lock_);
  ReduceCacheUsageLocked();
}

void GpuImageDecodeCache::ReduceCacheUsageLocked() NO_THREAD_SAFETY_ANALYSIS {
  EnsureCapacity(0);

  TryFlushPendingWork();
}

void GpuImageDecodeCache::SetShouldAggressivelyFreeResources(
    bool aggressively_free_resources) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::SetShouldAggressivelyFreeResources",
               "agressive_free_resources", aggressively_free_resources);
  if (aggressively_free_resources) {
    std::optional<viz::RasterContextProvider::ScopedRasterContextLock>
        context_lock;
    if (context_->GetLock()) {
      context_lock.emplace(context_);
    }

    base::AutoLock lock(lock_);
    aggressively_freeing_resources_ = aggressively_free_resources;
    EnsureCapacity(0);

    // We are holding the context lock, so finish cleaning up deleted images
    // now.
    RunPendingContextThreadOperations();
  } else {
    base::AutoLock lock(lock_);
    aggressively_freeing_resources_ = aggressively_free_resources;
  }
}

void GpuImageDecodeCache::ClearCache() {
  base::AutoLock lock(lock_);
  for (auto it = persistent_cache_.begin(); it != persistent_cache_.end();)
    it = RemoveFromPersistentCache(it);
  DCHECK(persistent_cache_.empty());
  paint_image_entries_.clear();

  TryFlushPendingWork();
}

void GpuImageDecodeCache::RecordStats() {
  base::AutoLock lock(lock_);
  double cache_usage;
  if (working_set_bytes_ > 0 &&
      base::CheckDiv(static_cast<double>(working_set_bytes_),
                     max_working_set_bytes_)
          .AssignIfValid(&cache_usage)) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Renderer4.GpuImageDecodeState.CachePeakUsagePercent",
        cache_usage * 100);
  }
}

void GpuImageDecodeCache::AddToPersistentCache(const DrawImage& draw_image,
                                               scoped_refptr<ImageData> data) {
  if (features::EnablePurgeGpuImageDecodeCache()) {
    DCHECK(persistent_cache_.empty() || has_pending_purge_task());
    PostPurgeOldCacheEntriesTask();
  }

  WillAddCacheEntry(draw_image);
  persistent_cache_memory_size_ += data->GetTotalSize();
  persistent_cache_.Put(draw_image.frame_key(), std::move(data));
}

template <typename Iterator>
Iterator GpuImageDecodeCache::RemoveFromPersistentCache(Iterator it) {
  if (it->second->decode.ref_count != 0 || it->second->upload.ref_count != 0) {
    // Orphan the image and erase it from the |persisent_cache_|. This ensures
    // that the image will be deleted once all refs are removed.
    it->second->is_orphaned = true;
  } else {
    // Current entry has no refs. Ensure it is not locked.
    DCHECK(!it->second->decode.is_locked());
    DCHECK(!it->second->upload.is_locked());

    // Unlocked images must not be budgeted.
    DCHECK(!it->second->is_budgeted);

    // Free the uploaded image if it exists.
    if (it->second->HasUploadedData())
      DeleteImage(it->second.get());
  }

  auto entries_it = paint_image_entries_.find(it->second->paint_image_id);
  CHECK(entries_it != paint_image_entries_.end());
  CHECK_GT(entries_it->second.count, 0u);

  // If this is the last entry for this image, remove its tracking.
  --entries_it->second.count;
  if (entries_it->second.count == 0u)
    paint_image_entries_.erase(entries_it);

  persistent_cache_memory_size_ -= it->second->GetTotalSize();
  return persistent_cache_.Erase(it);
}

bool GpuImageDecodeCache::TryFlushPendingWork() {
  // This is typically called when no tasks are running (between scheduling
  // tasks). Try to lock and run pending operations if possible, but don't
  // block on it.
  //
  // However, there are cases where the lock acquisition will fail. Indeed,
  // when a task runs on a worker thread, it may acquire both the compositor
  // lock then the GpuImageDecodeCache lock, whereas here we are trying to
  // acquire the compositor lock after. So the early exit is required to avoid
  // deadlocks.
  //
  // NO_THREAD_SAFETY_ANALYSIS: runtime-dependent locking.
  if (context_->GetLock() && !context_->GetLock()->Try()) {
    return false;
  }

  // The calls below will empty the cache on the GPU side. These calls will
  // also happen on the next frame, but we want to call them ourselves here to
  // avoid having to wait for the next frame (which might be a long wait/never
  // happen).
  RunPendingContextThreadOperations();
  context_->ContextSupport()->FlushPendingWork();
  // Transfer cache entries may have been deleted above (if
  // `ids_pending_deletion_` is not empty). But calling `FlushPendingWork()`
  // above is not enough, because it only deals with deferred messages, and
  // transfer cache entry deletion is *not* a deferred message. Rather, it is a
  // command buffer command, so we need to flush it. Otherwise if the page is
  // fully static, then no flush will come, and no entries will actually be
  // deleted. We only need a shallow flush because no glFlush() is required, we
  // merely need the deletion commands to be processed service-side.
  if (features::EnablePurgeGpuImageDecodeCache()) {
    context_->RasterInterface()->ShallowFlushCHROMIUM();
  }
  if (context_->GetLock()) {
    CheckContextLockAcquiredIfNecessary();
    context_->GetLock()->Release();
  }

  return true;
}

bool GpuImageDecodeCache::DoPurgeOldCacheEntries(base::TimeDelta max_age) {
  const base::TimeTicks min_last_use = base::TimeTicks::Now() - max_age;
  for (auto it = persistent_cache_.rbegin();
       it != persistent_cache_.rend() &&
       it->second->last_use <= min_last_use;) {
    if (it->second->decode.ref_count != 0 ||
        it->second->upload.ref_count != 0) {
      ++it;
      continue;
    }

    it = RemoveFromPersistentCache(it);
  }

  return TryFlushPendingWork();
}

void GpuImageDecodeCache::PurgeOldCacheEntriesCallback() {
  base::AutoLock locker(lock_);
  bool flushed_gpu_work = DoPurgeOldCacheEntries(get_max_purge_age());

  has_pending_purge_task_ = false;

  // If the cache is empty and we have flushed the pending work on the GPU side,
  // we stop posting the task, to avoid endless wakeups.
  if (persistent_cache_.empty() && flushed_gpu_work) {
    return;
  }

  PostPurgeOldCacheEntriesTask();
}

void GpuImageDecodeCache::PostPurgeOldCacheEntriesTask() {
  if (has_pending_purge_task()) {
    return;
  }

  if (task_runner_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuImageDecodeCache::PurgeOldCacheEntriesCallback,
                       weak_ptr_factory_.GetWeakPtr()),
        get_purge_interval());
    has_pending_purge_task_ = true;
  }
}

size_t GpuImageDecodeCache::GetMaximumMemoryLimitBytes() const {
  base::AutoLock locker(lock_);
  return max_working_set_bytes_;
}

bool GpuImageDecodeCache::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryAllocatorDumpGuid;
  using base::trace_event::MemoryDumpLevelOfDetail;

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::OnMemoryDump");

  base::AutoLock locker(lock_);

  std::string dump_name = base::StringPrintf(
      "cc/image_memory/cache_0x%" PRIXPTR, reinterpret_cast<uintptr_t>(this));

  if (args.level_of_detail == MemoryDumpLevelOfDetail::kBackground) {
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, working_set_bytes_);

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (const auto& image_pair : persistent_cache_) {
    const ImageData* image_data = image_pair.second.get();
    int image_id = static_cast<int>(image_pair.first.hash());

    // If we have discardable decoded data, dump this here.
    for (const auto aux_image : kAllAuxImages) {
      const auto& info = image_data->GetImageInfo(aux_image);
      const auto* data = image_data->decode.data(aux_image);
      if (!data) {
        continue;
      }
      std::string discardable_dump_name = base::StringPrintf(
          "%s/discardable/image_%d%s", dump_name.c_str(), image_id,
          aux_image == AuxImage::kDefault ? "" : AuxImageName(aux_image));
      MemoryAllocatorDump* dump =
          data->CreateMemoryAllocatorDump(discardable_dump_name.c_str(), pmd);
      // Dump the "locked_size" as an additional column.
      // This lets us see the amount of discardable which is contributing to
      // memory pressure.
      size_t locked_size = image_data->decode.is_locked() ? info.size : 0u;
      dump->AddScalar("locked_size", MemoryAllocatorDump::kUnitsBytes,
                      locked_size);
    }

    // If we have an uploaded image (that is actually on the GPU, not just a
    // CPU wrapper), upload it here.
    if (image_data->HasUploadedData()) {
      // TODO(lizeb): Include the right ID to link it with the GPU-side
      // resource.
      std::string uploaded_dump_name =
          base::StringPrintf("%s/gpu/image_%d", dump_name.c_str(),
                             image_data->upload.transfer_cache_id().value());
      MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(uploaded_dump_name);
      dump->AddScalar(MemoryAllocatorDump::kNameSize,
                      MemoryAllocatorDump::kUnitsBytes,
                      image_data->GetTotalSize());
    }
  }

  return true;
}

void GpuImageDecodeCache::DecodeImageInTask(const DrawImage& draw_image,
                                            TaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::DecodeImage");
  base::AutoLock lock(lock_);
  ImageData* image_data = GetImageDataForDrawImage(
      draw_image, InUseCacheKeyFromDrawImage(draw_image));
  DCHECK(image_data);
  DCHECK(image_data->is_budgeted) << "Must budget an image for pre-decoding";
  DecodeImageAndGenerateDarkModeFilterIfNecessary(draw_image, image_data,
                                                  task_type);
}

void GpuImageDecodeCache::UploadImageInTask(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UploadImage");
  std::optional<viz::RasterContextProvider::ScopedRasterContextLock>
      context_lock;
  if (context_->GetLock())
    context_lock.emplace(context_);

  base::AutoLock lock(lock_);

  auto cache_key = InUseCacheKeyFromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  DCHECK(image_data);
  DCHECK(image_data->is_budgeted) << "Must budget an image for pre-decoding";

  if (image_data->is_bitmap_backed)
    DecodeImageAndGenerateDarkModeFilterIfNecessary(draw_image, image_data,
                                                    TaskType::kInRaster);
  UploadImageIfNecessary(draw_image, image_data);
}

void GpuImageDecodeCache::OnImageDecodeTaskCompleted(
    const DrawImage& draw_image,
    TaskType task_type,
    ClientId client_id) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::OnImageDecodeTaskCompleted");
  base::AutoLock lock(lock_);
  auto cache_key = InUseCacheKeyFromDrawImage(draw_image);
  // Decode task is complete, remove our reference to it.
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  DCHECK(image_data);
  UMA_HISTOGRAM_BOOLEAN("Compositing.DecodeLCPCandidateImage.Hardware",
                        draw_image.paint_image().may_be_lcp_candidate());
  if (task_type == TaskType::kInRaster) {
    image_data->decode.task_map.erase(client_id);
  } else {
    image_data->decode.stand_alone_task_map.erase(client_id);
  }

  // While the decode task is active, we keep a ref on the decoded data.
  // Release that ref now.
  UnrefImageDecode(draw_image, cache_key);
}

void GpuImageDecodeCache::OnImageUploadTaskCompleted(
    const DrawImage& draw_image,
    ClientId client_id) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::OnImageUploadTaskCompleted");
  base::AutoLock lock(lock_);
  // Upload task is complete, remove our reference to it.
  InUseCacheKey cache_key = InUseCacheKeyFromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  DCHECK(image_data);
  image_data->upload.task_map.erase(client_id);

  // While the upload task is active, we keep a ref on both the image it will be
  // populating, as well as the decode it needs to populate it. Release these
  // refs now.
  UnrefImageDecode(draw_image, cache_key);
  UnrefImageInternal(draw_image, cache_key);
}

int GpuImageDecodeCache::CalculateUploadScaleMipLevel(
    const DrawImage& draw_image,
    AuxImage aux_image) const {
  // Images which are being clipped will have color-bleeding if scaled.
  // TODO(ericrk): Investigate uploading clipped images to handle this case and
  // provide further optimization. crbug.com/620899
  if (!enable_clipped_image_scaling_) {
    const bool is_clipped =
        draw_image.src_rect() !=
        SkIRect::MakeSize(draw_image.paint_image().GetSkISize(aux_image));
    if (is_clipped)
      return 0;
  }

  gfx::Size base_size = draw_image.paint_image().GetSize(aux_image);
  // Ceil our scaled size so that the mip map generated is guaranteed to be
  // larger. Take the abs of the scale, as mipmap functions don't handle
  // (and aren't impacted by) negative image dimensions.
  gfx::Size scaled_size =
      gfx::ScaleToCeiledSize(base_size, std::abs(draw_image.scale().width()),
                             std::abs(draw_image.scale().height()));

  return MipMapUtil::GetLevelForSize(base_size, scaled_size);
}

GpuImageDecodeCache::InUseCacheKey
GpuImageDecodeCache::InUseCacheKeyFromDrawImage(
    const DrawImage& draw_image) const {
  return InUseCacheKey(
      draw_image, CalculateUploadScaleMipLevel(draw_image, AuxImage::kDefault));
}

// Checks if an image decode needs a decode task and returns it.
scoped_refptr<TileTask> GpuImageDecodeCache::GetImageDecodeTaskAndRef(
    ClientId client_id,
    const DrawImage& draw_image,
    const TracingInfo& tracing_info,
    TaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetImageDecodeTaskAndRef");
  auto cache_key = InUseCacheKeyFromDrawImage(draw_image);
  bool for_raster = (task_type == TaskType::kInRaster);

  // This ref is kept alive while an upload task may need this decode. We
  // release this ref in UploadTaskCompleted.
  if (for_raster) {
    RefImageDecode(draw_image, cache_key);
  }

  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  DCHECK(image_data);

  // No decode is necessary for bitmap backed images.
  if (image_data->decode.is_locked() || image_data->is_bitmap_backed) {
    // We should never be creating a decode task for a not budgeted image.
    DCHECK(image_data->is_budgeted);
    // We should never be creating a decode for an already-uploaded image.
    DCHECK(!image_data->HasUploadedData());
    return nullptr;
  }

  // We didn't have an existing locked image, create a task to lock or decode.
  scoped_refptr<TileTask> result;

  ImageTaskMap& raster_task_map = image_data->decode.task_map;
  scoped_refptr<TileTask> raster_task =
      GetTaskFromMapForClientId(client_id, raster_task_map);
  ImageTaskMap& stand_alone_task_map = image_data->decode.stand_alone_task_map;
  scoped_refptr<TileTask> stand_alone_task =
      GetTaskFromMapForClientId(client_id, stand_alone_task_map);

  if (for_raster && raster_task) {
    result = std::move(raster_task);
  } else if (!for_raster && stand_alone_task) {
    result = std::move(stand_alone_task);
  } else {
    // Ref image decode and create a decode task. This ref will be released in
    // DecodeTaskCompleted.
    RefImageDecode(draw_image, cache_key);
    result = base::MakeRefCounted<GpuImageDecodeTaskImpl>(
        this, draw_image, tracing_info, task_type, client_id);
    if (for_raster) {
      raster_task_map[client_id] = result;
      if (stand_alone_task) {
        // If the existing stand-alone task hasn't started yet, make the new
        // raster task primary.
        if (stand_alone_task->state().IsNew()) {
          result->SetExternalDependent(stand_alone_task);
          image_data->RecordSpeculativeDecodeRasterTaskTakeover();
        } else {
          stand_alone_task->SetExternalDependent(result);
        }
      }
    } else {
      stand_alone_task_map[client_id] = result;
      if (raster_task && !raster_task->HasCompleted()) {
        raster_task->SetExternalDependent(result);
      }
    }
  }

  CHECK(result);
  return result;
}

void GpuImageDecodeCache::RefImageDecode(const DrawImage& draw_image,
                                         const InUseCacheKey& cache_key) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::RefImageDecode");
  auto found = in_use_cache_.find(cache_key);
  CHECK(found != in_use_cache_.end());
  ++found->second.ref_count;
  ++found->second.image_data->decode.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
}

void GpuImageDecodeCache::UnrefImageDecode(const DrawImage& draw_image,
                                           const InUseCacheKey& cache_key) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UnrefImageDecode");
  auto found = in_use_cache_.find(cache_key);
  CHECK(found != in_use_cache_.end());
  DCHECK_GT(found->second.image_data->decode.ref_count, 0u);
  DCHECK_GT(found->second.ref_count, 0u);
  --found->second.ref_count;
  --found->second.image_data->decode.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
  if (found->second.ref_count == 0u) {
    in_use_cache_.erase(found);
  }
}

void GpuImageDecodeCache::RefImage(const DrawImage& draw_image,
                                   const InUseCacheKey& cache_key) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::RefImage");
  auto found = in_use_cache_.find(cache_key);

  // If no secondary cache entry was found for the given |draw_image|, then
  // the draw_image only exists in the |persistent_cache_|. Create an in-use
  // cache entry now.
  if (found == in_use_cache_.end()) {
    auto found_image = persistent_cache_.Peek(draw_image.frame_key());
    CHECK(found_image != persistent_cache_.end());
    DCHECK(IsCompatible(found_image->second.get(), draw_image));
    found = in_use_cache_
                .insert(InUseCache::value_type(
                    cache_key, InUseCacheEntry(found_image->second)))
                .first;
  }

  CHECK(found != in_use_cache_.end());
  ++found->second.ref_count;
  ++found->second.image_data->upload.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
}

void GpuImageDecodeCache::UnrefImageInternal(const DrawImage& draw_image,
                                             const InUseCacheKey& cache_key) {
  auto found = in_use_cache_.find(cache_key);
  CHECK(found != in_use_cache_.end());
  DCHECK_GT(found->second.image_data->upload.ref_count, 0u);
  DCHECK_GT(found->second.ref_count, 0u);
  --found->second.ref_count;
  --found->second.image_data->upload.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
  if (found->second.ref_count == 0u) {
    in_use_cache_.erase(found);
  }
}

// Called any time an image or decode ref count changes. Takes care of any
// necessary memory budget book-keeping and cleanup.
void GpuImageDecodeCache::OwnershipChanged(const DrawImage& draw_image,
                                           ImageData* image_data) {
  bool has_any_refs =
      image_data->upload.ref_count > 0 || image_data->decode.ref_count > 0;
  // If we have no image refs on an image, we should unbudget it.
  if (!has_any_refs && image_data->is_budgeted) {
    DCHECK_GE(working_set_bytes_, image_data->GetTotalSize());
    DCHECK_GE(working_set_items_, 1u);
    working_set_bytes_ -= image_data->GetTotalSize();
    working_set_items_ -= 1;
    image_data->is_budgeted = false;
  }

  // Don't keep around completely empty images. This can happen if an image's
  // decode/upload tasks were both cancelled before completing.
  const bool has_cpu_data = image_data->decode.HasData() ||
                            (image_data->is_bitmap_backed &&
                             image_data->decode.image(0, AuxImage::kDefault));
  bool is_empty = !has_any_refs && !image_data->HasUploadedData() &&
                  !has_cpu_data && !image_data->is_orphaned;
  if (is_empty || draw_image.paint_image().no_cache()) {
    auto found_persistent = persistent_cache_.Peek(draw_image.frame_key());
    if (found_persistent != persistent_cache_.end())
      RemoveFromPersistentCache(found_persistent);
  }

  // Don't keep discardable cpu memory for GPU backed images. The cache hit rate
  // of the cpu fallback (in case we don't find this image in gpu memory) is
  // too low to cache this data.
  if (image_data->decode.ref_count == 0 &&
      image_data->HasUploadedData()) {
    image_data->decode.ResetData();
    image_data->speculative_decode_usage_stats_.reset();
  }

  // If we have no refs on an uploaded image, it should be unlocked. Do this
  // before any attempts to delete the image.
  if (image_data->upload.ref_count == 0 && image_data->upload.is_locked()) {
    UnlockImage(image_data);
  }

  // Don't keep around orphaned images.
  if (image_data->is_orphaned && !has_any_refs) {
    DeleteImage(image_data);
  }

  // If we have image that could be budgeted, but isn't, budget it now.
  if (has_any_refs && !image_data->is_budgeted &&
      CanFitInWorkingSet(image_data->GetTotalSize())) {
    working_set_bytes_ += image_data->GetTotalSize();
    working_set_items_ += 1;
    image_data->is_budgeted = true;
  }

  // We should unlock the decoded image memory for the image in two cases:
  // 1) The image is no longer being used (no decode or upload refs).
  // 2) This is a non-CPU image that has already been uploaded and we have
  //    no remaining decode refs.
  bool should_unlock_decode = !has_any_refs || (image_data->HasUploadedData() &&
                                                !image_data->decode.ref_count);

  if (should_unlock_decode && image_data->decode.is_locked()) {
    if (image_data->is_bitmap_backed) {
      DCHECK(!image_data->decode.HasData());
      image_data->decode.ResetBitmapImage();
    } else {
      DCHECK(image_data->decode.HasData());
      image_data->decode.Unlock();
    }
  }

  // EnsureCapacity to make sure we are under our cache limits.
  EnsureCapacity(0);

#if DCHECK_IS_ON()
  // Sanity check the above logic.
  if (!image_data->HasUploadedData()) {
    DCHECK(!image_data->is_budgeted || has_any_refs);
  }
#endif
}

// Checks whether we can fit a new image of size |required_size| in our
// working set. Also frees unreferenced entries to keep us below our preferred
// items limit.
bool GpuImageDecodeCache::EnsureCapacity(size_t required_size) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::EnsureCapacity");
  // While we are over preferred item capacity, we iterate through our set of
  // cached image data in LRU order, removing unreferenced images.
  for (auto it = persistent_cache_.rbegin();
       it != persistent_cache_.rend() && ExceedsCacheLimits();) {
    if (it->second->decode.ref_count != 0 ||
        it->second->upload.ref_count != 0) {
      ++it;
      continue;
    }

    it = RemoveFromPersistentCache(it);
  }

  return CanFitInWorkingSet(required_size);
}

bool GpuImageDecodeCache::CanFitInWorkingSet(size_t size) const {
  lock_.AssertAcquired();

  if (working_set_items_ >= max_working_set_items_)
    return false;

  base::CheckedNumeric<uint32_t> new_size(working_set_bytes_);
  new_size += size;
  if (!new_size.IsValid() || new_size.ValueOrDie() > max_working_set_bytes_)
    return false;

  return true;
}

bool GpuImageDecodeCache::ExceedsCacheLimits() const {
  size_t items_limit;
  if (aggressively_freeing_resources_) {
    items_limit = kSuspendedMaxItemsInCacheForGpu;
  } else {
    items_limit = kNormalMaxItemsInCacheForGpu;
  }

  return persistent_cache_.size() > items_limit;
}

bool GpuImageDecodeCache::NeedsDarkModeFilter(const DrawImage& draw_image,
                                              ImageData* image_data) {
  DCHECK(image_data);

  // |draw_image| does not need dark mode to be applied.
  if (!draw_image.use_dark_mode())
    return false;

  // |dark_mode_filter_| must be valid, if |draw_image| has use_dark_mode set.
  DCHECK(dark_mode_filter_);

  // TODO(prashant.n): RSDM - Add support for YUV decoded data.
  if (image_data->info.yuva.has_value()) {
    return false;
  }

  // Dark mode filter is already generated and cached.
  if (base::Contains(image_data->decode.dark_mode_color_filter_cache,
                     draw_image.src_rect())) {
    return false;
  }

  return true;
}

void GpuImageDecodeCache::DecodeImageAndGenerateDarkModeFilterIfNecessary(
    const DrawImage& draw_image,
    ImageData* image_data,
    TaskType task_type) {
  // Check if image needs dark mode to be applied, based on this image may be
  // decoded again if decoded data is not available.
  bool needs_dark_mode_filter = NeedsDarkModeFilter(draw_image, image_data);
  DecodeImageIfNecessary(draw_image, image_data, task_type,
                         needs_dark_mode_filter);
  if (needs_dark_mode_filter)
    GenerateDarkModeFilter(draw_image, image_data);
}

void GpuImageDecodeCache::DecodeImageIfNecessary(
    const DrawImage& draw_image,
    ImageData* image_data,
    TaskType task_type,
    bool needs_decode_for_dark_mode) {
  DCHECK_GT(image_data->decode.ref_count, 0u);

  if (image_data->decode.decode_failure) {
    // We have already tried and failed to decode this image. Don't try again.
    return;
  }

  if (image_data->HasUploadedData() &&
      TryLockImage(HaveContextLock::kNo, draw_image, image_data) &&
      !needs_decode_for_dark_mode) {
    // We already have an uploaded image and we don't need a decode for dark
    // mode too, so no reason to decode.
    return;
  }

  if (image_data->is_bitmap_backed) {
    DCHECK(!draw_image.paint_image().IsLazyGenerated());
    if (image_data->info.yuva.has_value()) {
      NOTREACHED() << "YUV + Bitmap is unknown and unimplemented!";
    } else {
      image_data->decode.SetBitmapImage(
          draw_image.paint_image().GetSwSkImage());
    }
    return;
  }

  if (image_data->decode.HasData() &&
      (image_data->decode.is_locked() || image_data->decode.Lock())) {
    // We already decoded this, or we just needed to lock, early out.
    return;
  }

  if (image_data->IsSpeculativeDecode()) {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("loading"),
                         "SpeculativeImageDecodeRun", TRACE_EVENT_SCOPE_THREAD,
                         "image_id", image_data->paint_image_id);
  }
  TRACE_EVENT1("cc,benchmark", "GpuImageDecodeCache::DecodeImage",
               "paint_image_id", image_data->paint_image_id);

  image_data->decode.ResetData();

  // Prevent image_data from being deleted while lock is not held.
  scoped_refptr<ImageData> image_data_holder(image_data);

  // Decode the image into `aux_image_data` while the lock is not held.
  std::array<DecodedAuxImageData, kAuxImageCount> aux_image_data;
  {
    base::AutoUnlock unlock(lock_);
    for (auto aux_image : kAllAuxImages) {
      if (aux_image == AuxImage::kGainmap) {
        if (!draw_image.paint_image().HasGainmapInfo()) {
          continue;
        }
      }
      const auto aux_image_index = AuxImageIndex(aux_image);
      const auto info = image_data->GetImageInfo(aux_image);

      // Allocate the backing memory for the decode.
      std::unique_ptr<base::DiscardableMemory> backing_memory;
      if (base::FeatureList::IsEnabled(
              features::kNoDiscardableMemoryForGpuDecodePath)) {
        backing_memory = std::make_unique<HeapDiscardableMemory>(info.size);
      } else {
        auto* allocator = base::DiscardableMemoryAllocator::GetInstance();
        backing_memory =
            allocator->AllocateLockedDiscardableMemoryWithRetryOrDie(
                info.size, base::BindOnce(&GpuImageDecodeCache::ClearCache,
                                          base::Unretained(this)));
      }

      // Do the decode.
      if (info.yuva.has_value()) {
        // Decode as YUV.
        DCHECK(!info.rgba.has_value());
        DVLOG(3) << "GpuImageDecodeCache (" << AuxImageName(aux_image)
                 << "wants to do YUV decoding/rendering";
        SkYUVAPixmaps yuva_pixmaps = SkYUVAPixmaps::FromExternalMemory(
            info.yuva.value(), backing_memory->data());
        if (DrawAndScaleImageYUV(draw_image, aux_image, generator_client_id_,
                                 yuva_supported_data_types_, yuva_pixmaps)) {
          aux_image_data[aux_image_index] =
              DecodedAuxImageData(yuva_pixmaps, std::move(backing_memory));
        } else {
          DLOG(ERROR) << "DrawAndScaleImageYUV failed.";
          backing_memory->Unlock();
          backing_memory.reset();
          break;
        }
      } else {
        // Decode as RGB.
        DCHECK(info.rgba.has_value());
        SkImageInfo image_info =
            info.rgba->makeColorSpace(ColorSpaceForImageDecode(draw_image));
        SkPixmap pixmap(image_info, backing_memory->data(),
                        image_info.minRowBytes());
        if (DrawAndScaleImageRGB(draw_image, aux_image, pixmap,
                                 generator_client_id_)) {
          aux_image_data[aux_image_index] =
              DecodedAuxImageData(pixmap, std::move(backing_memory));
        } else {
          DLOG(ERROR) << "DrawAndScaleImageRGB failed.";
          backing_memory->Unlock();
          backing_memory.reset();
          break;
        }
      }
    }
  }

  if (image_data->decode.HasData()) {
    // An at-raster task decoded this before us. Ignore our decode, but ensure
    // that the expected number of images are populated.
    for (auto aux_image : kAllAuxImages) {
      const auto info = image_data->GetImageInfo(aux_image);
      int num_planes = 0;
      if (info.yuva) {
        num_planes = image_data->info.yuva->numPlanes();
      }
      if (info.rgba) {
        num_planes = 1;
      }
      for (int i = 0; i < SkYUVAInfo::kMaxPlanes; ++i) {
        if (i < num_planes) {
          DCHECK(image_data->decode.image(i, aux_image));
        } else {
          DCHECK(!image_data->decode.image(i, aux_image));
        }
      }
    }
    return;
  }

  // If the default image's `data` was not populated, we had a non-decodable
  // image. Do not fail if the gainmap failed to decode.
  if (!aux_image_data[kAuxImageIndexDefault].data) {
    image_data->decode.decode_failure = true;
    return;
  }

  image_data->decode.SetLockedData(aux_image_data,
                                   task_type == TaskType::kOutOfRaster);
}

void GpuImageDecodeCache::GenerateDarkModeFilter(const DrawImage& draw_image,
                                                 ImageData* image_data) {
  DCHECK(dark_mode_filter_);
  // Caller must ensure draw image needs dark mode to be applied.
  DCHECK(NeedsDarkModeFilter(draw_image, image_data));
  // Caller must ensure image is valid and has decoded data.
  DCHECK(image_data->decode.image(0, AuxImage::kDefault));

  // TODO(prashant.n): Calling ApplyToImage() from |dark_mode_filter_| can be
  // expensive. Check the possibilitiy of holding |lock_| only for accessing and
  // storing dark mode result on |image_data|.
  lock_.AssertAcquired();

  if (image_data->decode.decode_failure)
    return;

  sk_sp<ColorFilter> filter;
  const SkPixmap& pixmap = image_data->decode.pixmaps(AuxImage::kDefault)[0];
  const SkIRect& src = draw_image.src_rect();

  if (base::FeatureList::IsEnabled(features::kUnlockDuringGpuImageOperations)) {
    // Release the lock while calling ApplyToImage as it can be expensive.
    scoped_refptr<ImageData> image_data_ref(image_data);
    base::AutoUnlock unlock(lock_);
    filter = dark_mode_filter_->ApplyToImage(pixmap, src);
  } else {
    filter = dark_mode_filter_->ApplyToImage(pixmap, src);
  }

  image_data->decode.dark_mode_color_filter_cache[src] = std::move(filter);
}

void GpuImageDecodeCache::UploadImageIfNecessary(const DrawImage& draw_image,
                                                 ImageData* image_data) {
  CheckContextLockAcquiredIfNecessary();
  // We are about to upload a new image and are holding the context lock.
  // Ensure that any images which have been marked for deletion are actually
  // cleaned up so we don't exceed our memory limit during this upload.
  RunPendingContextThreadOperations();

  if (image_data->decode.decode_failure) {
    // We were unable to decode this image. Don't try to upload.
    return;
  }

  // If an upload already exists, try to lock it. If this fails, it will clear
  // any uploaded data.
  if (image_data->HasUploadedData())
    TryLockImage(HaveContextLock::kYes, draw_image, image_data);

  // Ensure the mip status is correct before returning the locked upload or
  // preparing to upload a new image.
  if (!image_data->needs_mips) {
    image_data->needs_mips = ShouldGenerateMips(
        draw_image, AuxImage::kDefault, image_data->upload_scale_mip_level);
  }

  // If we have uploaded data at this point, it is locked with correct mips,
  // just return.
  if (image_data->HasUploadedData())
    return;

  TRACE_EVENT0("cc", "GpuImageDecodeCache::UploadImage");
  DCHECK(image_data->decode.is_locked());
  image_data->decode.mark_used();
  DCHECK_GT(image_data->decode.ref_count, 0u);
  DCHECK_GT(image_data->upload.ref_count, 0u);

  // Let `target_color_space` be the color space that the image is converted to
  // for storage in the cache. If it is nullptr then no conversion is performed,
  // and the decoded color space is used.
  sk_sp<SkColorSpace> target_color_space =
      SupportsColorSpaceConversion() &&
              draw_image.target_color_space().IsValid()
          ? draw_image.target_color_space().ToSkColorSpace()
          : nullptr;
  // Let `decoded_color_space` be the color space that the decoded image is in.
  // This takes into account the fact that we might need to ignore an embedded
  // image color space if `color_type_` does not support color space
  // conversions or that some color conversion might have happened at decode
  // time.
  sk_sp<SkColorSpace> decoded_color_space =
      ColorSpaceForImageDecode(draw_image);
  if (target_color_space && decoded_color_space &&
      SkColorSpace::Equals(target_color_space.get(),
                           decoded_color_space.get())) {
    target_color_space = nullptr;
  }
  // Do not color convert images that are YUV or might be tone mapped.
  if (image_data->info.yuva.has_value() ||
      draw_image.paint_image().HasGainmapInfo() ||
      ToneMapUtil::UseGlobalToneMapFilter(decoded_color_space.get())) {
    target_color_space = nullptr;
  }
  const std::optional<gfx::HDRMetadata> hdr_metadata =
      draw_image.paint_image().GetHDRMetadata();

  std::array<ClientImageTransferCacheEntry::Image, kAuxImageCount> image;
  bool has_gainmap = false;

  for (auto aux_image : kAllAuxImages) {
    auto aux_image_index = AuxImageIndex(aux_image);
    const auto& info = image_data->GetImageInfo(aux_image);
    if (aux_image == AuxImage::kGainmap) {
      // The gainmap image is allowed to silently fail to decode. If that
      // happens, there will be no data. Just pretend it didn't exist.
      if (!image_data->decode.data(aux_image)) {
        continue;
      }
      has_gainmap = info.rgba.has_value() || info.yuva.has_value();
    }
    if (info.yuva.has_value()) {
      DCHECK(!info.rgba.has_value());
      image[aux_image_index] = ClientImageTransferCacheEntry::Image(
          image_data->decode.pixmaps(aux_image), info.yuva->yuvaInfo(),
          decoded_color_space.get());
    }
    if (info.rgba.has_value()) {
      DCHECK(!info.yuva.has_value());
      image[aux_image_index] = ClientImageTransferCacheEntry::Image(
          &image_data->decode.pixmaps(aux_image)[0]);
    }
  }

  ClientImageTransferCacheEntry image_entry =
      has_gainmap
          ? ClientImageTransferCacheEntry(
                image[kAuxImageIndexDefault], image[kAuxImageIndexGainmap],
                draw_image.paint_image().GetGainmapInfo(),
                image_data->needs_mips)
          : ClientImageTransferCacheEntry(image[kAuxImageIndexDefault],
                                          image_data->needs_mips, hdr_metadata,
                                          target_color_space);
  if (!image_entry.IsValid())
    return;

  scoped_refptr<ImageData> image_data_holder(image_data);
  bool uploaded = false;
  auto upload_image_entry_func = [&image_entry, &uploaded, this]() {
    uint32_t size = image_entry.SerializedSize();
    base::span<uint8_t> data =
        context_->ContextSupport()->MapTransferCacheEntry(size);
    if (!data.empty()) {
      bool succeeded = image_entry.Serialize(data);
      DCHECK(succeeded);
      context_->ContextSupport()->UnmapAndCreateTransferCacheEntry(
          image_entry.UnsafeType(), image_entry.Id());
      uploaded = true;
    }
  };

  if (base::FeatureList::IsEnabled(features::kUnlockDuringGpuImageOperations)) {
    base::AutoUnlock unlock(lock_);
    upload_image_entry_func();
  } else {
    upload_image_entry_func();
  }

  if (uploaded) {
    // If we unlocked during the upload, another thread may have uploaded the
    // image while we were working. If that happened, we should simply delete
    // the entry we just created, as it is now redundant.
    if (image_data->HasUploadedData()) {
      context_->ContextSupport()->DeleteTransferCacheEntry(
          image_entry.UnsafeType(), image_entry.Id());
    } else {
      image_data->upload.SetTransferCacheId(image_entry.Id());
    }
  } else {
    // Transfer cache entry can fail due to a lost gpu context or failure
    // to allocate shared memory.  Handle this gracefully.  Mark this
    // image as "decode failed" so that we do not try to handle it again.
    // If this was a lost context, we'll recreate this image decode cache.
    image_data->decode.decode_failure = true;
  }
}

scoped_refptr<GpuImageDecodeCache::ImageData>
GpuImageDecodeCache::CreateImageData(const DrawImage& draw_image,
                                     bool speculative_decode) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::CreateImageData");
  std::array<ImageInfo, kAuxImageCount> image_info;

  // Extract ImageInfo and SkImageInfo for the default image, assuming software
  // decoding to RGBA.
  const auto [sk_image_info, upload_scale_mip_level] =
      CreateImageInfoForDrawImage(draw_image, AuxImage::kDefault);
  image_info[kAuxImageIndexDefault] = ImageInfo(sk_image_info);
  bool needs_mips = ShouldGenerateMips(draw_image, AuxImage::kDefault,
                                       upload_scale_mip_level);

  // Extract ImageInfo and SkImageInfo for the gainmap image, if it exists,
  // assuming software decoindg to RGBA.
  const bool has_gainmap = draw_image.paint_image().HasGainmapInfo();
  SkImageInfo gainmap_sk_image_info;
  ImageInfo gainmap_info;
  if (has_gainmap) {
    gainmap_sk_image_info = std::get<0>(
        CreateImageInfoForDrawImage(draw_image, AuxImage::kGainmap));
    image_info[kAuxImageIndexGainmap] = ImageInfo(gainmap_sk_image_info);
  }

  // Determine if the image can fit in a texture (to determine mode and RGBA vs
  // YUVA decode).
  const bool image_larger_than_max_texture =
      sk_image_info.width() > max_texture_size_ ||
      sk_image_info.height() > max_texture_size_ ||
      (has_gainmap && (gainmap_sk_image_info.width() > max_texture_size_ ||
                       gainmap_sk_image_info.height() > max_texture_size_));
  // We need to cache the result of color conversion on the cpu if the image
  // will be color converted during the decode.
  auto decode_color_space = ColorSpaceForImageDecode(draw_image);
  const bool cache_color_conversion_on_cpu =
      decode_color_space &&
      !SkColorSpace::Equals(decode_color_space.get(),
                            draw_image.paint_image().color_space());

  // |is_bitmap_backed| specifies whether the image has pixel data which can
  // directly be used for the upload. This will be the case for non-lazy images
  // used at the original scale. In these cases, we don't internally cache any
  // cpu component for the image.
  // However, if the image will be scaled or color converts on the cpu, we
  // consider it a lazy image and cache the scaled result in discardable memory.
  const bool is_bitmap_backed = !draw_image.paint_image().IsLazyGenerated() &&
                                upload_scale_mip_level == 0 &&
                                !cache_color_conversion_on_cpu;

  // Determine if we will do YUVA decoding for the image and the gainmap, and
  // update `image_info` to reflect that.
  if (!image_larger_than_max_texture) {
    auto yuva_info = GetYUVADecodeInfo(draw_image, AuxImage::kDefault,
                                       sk_image_info.dimensions(),
                                       yuva_supported_data_types_);
    if (yuva_info.has_value()) {
      image_info[kAuxImageIndexDefault] = ImageInfo(yuva_info.value());
    }
    if (has_gainmap) {
      auto gainmap_yuva_info = GetYUVADecodeInfo(
          draw_image, AuxImage::kGainmap, gainmap_sk_image_info.dimensions(),
          yuva_supported_data_types_);
      if (gainmap_yuva_info.has_value()) {
        image_info[kAuxImageIndexGainmap] =
            ImageInfo(gainmap_yuva_info.value());
      }
    }
  }

  return base::WrapRefCounted(new ImageData(
      draw_image.paint_image().stable_id(), draw_image.target_color_space(),
      CalculateDesiredFilterQuality(draw_image), upload_scale_mip_level,
      needs_mips, is_bitmap_backed, speculative_decode, image_info));
}

void GpuImageDecodeCache::WillAddCacheEntry(const DrawImage& draw_image) {
  // Remove any old entries for this image. We keep at-most 2 ContentIds for a
  // PaintImage (pending and active tree).
  auto& cache_entries =
      paint_image_entries_[draw_image.paint_image().stable_id()];
  cache_entries.count++;

  auto& cached_content_ids = cache_entries.content_ids;
  const PaintImage::ContentId new_content_id =
      draw_image.frame_key().content_id();

  if (cached_content_ids[0] == new_content_id ||
      cached_content_ids[1] == new_content_id) {
    return;
  }

  if (cached_content_ids[0] == PaintImage::kInvalidContentId) {
    cached_content_ids[0] = new_content_id;
    return;
  }

  if (cached_content_ids[1] == PaintImage::kInvalidContentId) {
    cached_content_ids[1] = new_content_id;
    return;
  }

  const PaintImage::ContentId content_id_to_remove =
      std::min(cached_content_ids[0], cached_content_ids[1]);
  const PaintImage::ContentId content_id_to_keep =
      std::max(cached_content_ids[0], cached_content_ids[1]);
  DCHECK_NE(content_id_to_remove, content_id_to_keep);

  for (auto it = persistent_cache_.begin(); it != persistent_cache_.end();) {
    if (it->first.content_id() != content_id_to_remove) {
      ++it;
    } else {
      it = RemoveFromPersistentCache(it);
    }
  }

  // Removing entries from the persistent cache should not erase the tracking
  // for the current paint_image, since we have 2 different content ids for it
  // and only one of them was erased above.
  DCHECK_NE(paint_image_entries_.count(draw_image.paint_image().stable_id()),
            0u);

  cached_content_ids[0] = content_id_to_keep;
  cached_content_ids[1] = new_content_id;
}

void GpuImageDecodeCache::DeleteImage(ImageData* image_data) {
  if (image_data->HasUploadedData()) {
    DCHECK(!image_data->upload.is_locked());
    ids_pending_deletion_.push_back(*image_data->upload.transfer_cache_id());
  }
  image_data->upload.Reset();
}

void GpuImageDecodeCache::UnlockImage(ImageData* image_data) {
  DCHECK(image_data->HasUploadedData());
  ids_pending_unlock_.push_back(*image_data->upload.transfer_cache_id());
  image_data->upload.OnUnlock();
  }

void GpuImageDecodeCache::RunPendingContextThreadOperations() {
  CheckContextLockAcquiredIfNecessary();

  for (auto id : ids_pending_unlock_) {
    context_->ContextSupport()->UnlockTransferCacheEntries({std::make_pair(
        static_cast<uint32_t>(TransferCacheEntryType::kImage), id)});
  }
  ids_pending_unlock_.clear();

  for (auto id : ids_pending_deletion_) {
    if (context_->ContextSupport()->ThreadsafeLockTransferCacheEntry(
            static_cast<uint32_t>(TransferCacheEntryType::kImage), id)) {
      context_->ContextSupport()->DeleteTransferCacheEntry(
          static_cast<uint32_t>(TransferCacheEntryType::kImage), id);
    }
  }
  ids_pending_deletion_.clear();
}

std::tuple<SkImageInfo, int> GpuImageDecodeCache::CreateImageInfoForDrawImage(
    const DrawImage& draw_image,
    AuxImage aux_image) const {
  const int upload_scale_mip_level =
      CalculateUploadScaleMipLevel(draw_image, aux_image);
  gfx::Size mip_size =
      CalculateSizeForMipLevel(draw_image, aux_image, upload_scale_mip_level);

  // Decide the SkColorType for the buffer for the PaintImage to draw or
  // decode into. Default to using the cache's color type.
  SkColorType color_type = color_type_;

  // The PaintImage will identify that its content is high bit depth by setting
  // its SkColorType to kRGBA_F16_SkColorType. Always decode high bit depth WCG
  // and HDR content as high bit depth, to avoid quantization artifacts.
  // https://crbug.com/1363056: See effects of tone mapping applied to dithered
  // low bit depth images.
  // https://crbug.com/1266456: Do not attempt to decode non high bit depth
  // images as high bit depth or they might not appear.
  // https://crbug.com/1076568: See historical discussions.
  const auto image_color_type =
      draw_image.paint_image().GetSkImageInfo(aux_image).colorType();
  if (image_color_type == kRGBA_F16_SkColorType &&
      draw_image.paint_image().GetContentColorUsage() !=
          gfx::ContentColorUsage::kSRGB) {
    color_type = kRGBA_F16_SkColorType;
  }

  return {SkImageInfo::Make(mip_size.width(), mip_size.height(), color_type,
                            kPremul_SkAlphaType),
          upload_scale_mip_level};
}

bool GpuImageDecodeCache::TryLockImage(HaveContextLock have_context_lock,
                                       const DrawImage& draw_image,
                                       ImageData* data) {
  DCHECK(data->HasUploadedData());

  if (data->upload.is_locked())
    return true;

  DCHECK(data->upload.transfer_cache_id());
  if (context_->ContextSupport()->ThreadsafeLockTransferCacheEntry(
          static_cast<uint32_t>(TransferCacheEntryType::kImage),
          *data->upload.transfer_cache_id())) {
    data->upload.OnLock();
    return true;
  }

  // Couldn't lock, abandon the image.
  DeleteImage(data);
  return false;
}

// Tries to find an ImageData that can be used to draw the provided
// |draw_image|. First looks for an exact entry in our |in_use_cache_|. If one
// cannot be found, it looks for a compatible entry in our |persistent_cache_|.
GpuImageDecodeCache::ImageData* GpuImageDecodeCache::GetImageDataForDrawImage(
    const DrawImage& draw_image,
    const InUseCacheKey& key,
    bool record_speculative_decode_stats) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetImageDataForDrawImage");
  DCHECK(UseCacheForDrawImage(draw_image));

  auto found_in_use = in_use_cache_.find(key);
  if (found_in_use != in_use_cache_.end()) {
    scoped_refptr<ImageData>& image_data = found_in_use->second.image_data;
    if (image_data->IsSpeculativeDecode() && record_speculative_decode_stats) {
      if (!image_data->SpeculativeDecodeHasMatched()) {
        TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("loading"),
                             "SpeculativeImageDecodeInUseMatch",
                             TRACE_EVENT_SCOPE_THREAD, "image_id",
                             image_data->paint_image_id, "raster_mip_level",
                             key.mip_level());
      }
      image_data->RecordSpeculativeDecodeMatch(
          image_data->upload_scale_mip_level);
    }
    return image_data.get();
  }

  auto found_persistent = persistent_cache_.Get(draw_image.frame_key());
  if (found_persistent != persistent_cache_.end()) {
    scoped_refptr<ImageData>& image_data = found_persistent->second;
    bool first_match = !image_data->SpeculativeDecodeHasMatched();
    if (image_data->IsSpeculativeDecode() && record_speculative_decode_stats) {
      image_data->RecordSpeculativeDecodeMatch(key.mip_level());
    }
    if (IsCompatible(image_data.get(), draw_image)) {
      image_data->last_use = base::TimeTicks::Now();
      if (image_data->IsSpeculativeDecode() &&
          record_speculative_decode_stats) {
        if (first_match) {
          TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("loading"),
                               "SpeculativeImageDecodeCompatibleMatch",
                               TRACE_EVENT_SCOPE_THREAD, "image_id",
                               image_data->paint_image_id, "raster_mip_level",
                               key.mip_level());
        }
      }
      return image_data.get();
    } else {
      if (image_data->IsSpeculativeDecode() &&
          record_speculative_decode_stats) {
        TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("loading"),
                             "SpeculativeImageDecodeIncompatibleMatch",
                             TRACE_EVENT_SCOPE_THREAD, "image_id",
                             image_data->paint_image_id, "raster_mip_level",
                             key.mip_level());
      }
      RemoveFromPersistentCache(found_persistent);
    }
  }

  return nullptr;
}

// Determines if we can draw the provided |draw_image| using the provided
// |image_data|. This is true if the |image_data| is not scaled, or if it
// is scaled at an equal or larger scale and equal or larger quality to
// the provided |draw_image|.
bool GpuImageDecodeCache::IsCompatible(const ImageData* image_data,
                                       const DrawImage& draw_image) const {
  const bool is_scaled = image_data->upload_scale_mip_level != 0;
  const bool scale_is_compatible =
      CalculateUploadScaleMipLevel(draw_image, AuxImage::kDefault) >=
      image_data->upload_scale_mip_level;
  auto desired_quality = CalculateDesiredFilterQuality(draw_image);
  bool quality_is_compatible = desired_quality <= image_data->quality;
  if (base::FeatureList::IsEnabled(
          features::kPreserveDiscardableImageMapQuality)) {
    // Nearest neighbor is used for `image-rendering: pixelated` which is not
    // compatible with higher qualities.
    if (desired_quality == PaintFlags::FilterQuality::kNone &&
        image_data->quality != PaintFlags::FilterQuality::kNone) {
      quality_is_compatible = false;
    }
  }
  if (is_scaled && (!scale_is_compatible || !quality_is_compatible)) {
    return false;
  }

  // This is overly pessimistic. If the image is tone mapped or decoded to
  // YUV, then the target color space is ignored anyway.
  const bool color_is_compatible =
      image_data->target_color_space == draw_image.target_color_space();
  if (!color_is_compatible)
    return false;

  return true;
}

size_t GpuImageDecodeCache::GetDrawImageSizeForTesting(const DrawImage& image) {
  base::AutoLock lock(lock_);
  scoped_refptr<ImageData> data =
      CreateImageData(image, false /* speculative_decode */);
  return data->GetTotalSize();
}

void GpuImageDecodeCache::SetImageDecodingFailedForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.frame_key());
  CHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  image_data->decode.decode_failure = true;
}

bool GpuImageDecodeCache::DiscardableIsLockedForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.frame_key());
  CHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  return image_data->decode.is_locked();
}

bool GpuImageDecodeCache::IsInInUseCacheForTesting(
    const DrawImage& image) const {
  base::AutoLock locker(lock_);
  auto found = in_use_cache_.find(InUseCacheKeyFromDrawImage(image));
  return found != in_use_cache_.end();
}

bool GpuImageDecodeCache::IsInPersistentCacheForTesting(
    const DrawImage& image) const {
  base::AutoLock locker(lock_);
  auto found = persistent_cache_.Peek(image.frame_key());
  return found != persistent_cache_.end();
}

sk_sp<SkImage> GpuImageDecodeCache::GetSWImageDecodeForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.frame_key());
  CHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  DCHECK(!image_data->info.yuva.has_value());
  return image_data->decode.ImageForTesting();
}

size_t GpuImageDecodeCache::GetDarkModeImageCacheSizeForTesting(
    const DrawImage& draw_image) {
  base::AutoLock lock(lock_);
  ImageData* image_data = GetImageDataForDrawImage(
      draw_image, InUseCacheKeyFromDrawImage(draw_image));
  return image_data ? image_data->decode.dark_mode_color_filter_cache.size()
                    : 0u;
}

bool GpuImageDecodeCache::NeedsDarkModeFilterForTesting(
    const DrawImage& draw_image) {
  base::AutoLock lock(lock_);
  ImageData* image_data = GetImageDataForDrawImage(
      draw_image, InUseCacheKeyFromDrawImage(draw_image));

  return NeedsDarkModeFilter(draw_image, image_data);
}

void GpuImageDecodeCache::TouchCacheEntryForTesting(
    const DrawImage& draw_image) {
  base::AutoLock locker(lock_);
  ImageData* image_data = GetImageDataForDrawImage(
      draw_image, InUseCacheKeyFromDrawImage(draw_image));
  image_data->last_use = base::TimeTicks::Now();
}

void GpuImageDecodeCache::OnMemoryPressure(base::MemoryPressureLevel level) {
  if (!ImageDecodeCacheUtils::ShouldEvictCaches(level))
    return;

  base::AutoLock lock(lock_);
  base::AutoReset<bool> reset(&aggressively_freeing_resources_, true);
  ReduceCacheUsageLocked();
}

bool GpuImageDecodeCache::AcquireContextLockForTesting() {
  if (!context_->GetLock()) {
    return false;
  }
  return context_->GetLock()->Try();
}

void GpuImageDecodeCache::ReleaseContextLockForTesting()
    NO_THREAD_SAFETY_ANALYSIS {
  if (!context_->GetLock()) {
    return;
  }
  context_->GetLock()->Release();
}

bool GpuImageDecodeCache::SupportsColorSpaceConversion() const {
  switch (color_type_) {
    case kRGBA_8888_SkColorType:
    case kBGRA_8888_SkColorType:
    case kRGBA_F16_SkColorType:
      return true;
    default:
      return false;
  }
}

sk_sp<SkColorSpace> GpuImageDecodeCache::ColorSpaceForImageDecode(
    const DrawImage& image) const {
  if (!SupportsColorSpaceConversion())
    return nullptr;

  // For kGpu or kTransferCache images color conversion is handled during
  // upload, so keep the original colorspace here.
  return sk_ref_sp(image.paint_image().color_space());
}

void GpuImageDecodeCache::CheckContextLockAcquiredIfNecessary() {
  if (!context_->GetLock())
    return;
  context_->GetLock()->AssertAcquired();
}

// static
scoped_refptr<TileTask> GpuImageDecodeCache::GetTaskFromMapForClientId(
    const ClientId client_id,
    const ImageTaskMap& task_map) {
  auto task_it = std::ranges::find_if(
      task_map,
      [client_id](
          const std::pair<ClientId, scoped_refptr<TileTask>> task_item) {
        return client_id == task_item.first;
      });
  if (task_it != task_map.end())
    return task_it->second;
  return nullptr;
}

base::TimeDelta GpuImageDecodeCache::get_purge_interval() {
  return base::Seconds(30);
}

base::TimeDelta GpuImageDecodeCache::get_max_purge_age() {
  return base::Seconds(30);
}

}  // namespace cc
