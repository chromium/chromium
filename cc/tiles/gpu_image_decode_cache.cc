// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/gpu_image_decode_cache.h"

#include <inttypes.h>

#include "base/auto_reset.h"
#include "base/debug/alias.h"
#include "base/hash.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/histograms.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/raster/scoped_grcontext_access.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/mipmap_util.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "ui/gfx/skia_util.h"
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
          SkIRect::MakeWH(draw_image.paint_image().width(),
                          draw_image.paint_image().height()))) {
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
SkFilterQuality CalculateDesiredFilterQuality(const DrawImage& draw_image) {
  return std::min(kMedium_SkFilterQuality, draw_image.filter_quality());
}

// Calculate the mip level to upload-scale the image to before uploading. We use
// mip levels rather than exact scales to increase re-use of scaled images.
int CalculateUploadScaleMipLevel(const DrawImage& draw_image) {
  // Images which are being clipped will have color-bleeding if scaled.
  // TODO(ericrk): Investigate uploading clipped images to handle this case and
  // provide further optimization. crbug.com/620899
  if (draw_image.src_rect() !=
      SkIRect::MakeWH(draw_image.paint_image().width(),
                      draw_image.paint_image().height())) {
    return 0;
  }

  gfx::Size base_size(draw_image.paint_image().width(),
                      draw_image.paint_image().height());
  // Ceil our scaled size so that the mip map generated is guaranteed to be
  // larger. Take the abs of the scale, as mipmap functions don't handle
  // (and aren't impacted by) negative image dimensions.
  gfx::Size scaled_size =
      gfx::ScaleToCeiledSize(base_size, std::abs(draw_image.scale().width()),
                             std::abs(draw_image.scale().height()));

  return MipMapUtil::GetLevelForSize(base_size, scaled_size);
}

// Calculates the scale factor which can be used to scale an image to a given
// mip level.
SkSize CalculateScaleFactorForMipLevel(const DrawImage& draw_image,
                                       int upload_scale_mip_level) {
  gfx::Size base_size(draw_image.paint_image().width(),
                      draw_image.paint_image().height());
  return MipMapUtil::GetScaleAdjustmentForLevel(base_size,
                                                upload_scale_mip_level);
}

// Calculates the size of a given mip level.
gfx::Size CalculateSizeForMipLevel(const DrawImage& draw_image,
                                   int upload_scale_mip_level) {
  gfx::Size base_size(draw_image.paint_image().width(),
                      draw_image.paint_image().height());
  return MipMapUtil::GetSizeForLevel(base_size, upload_scale_mip_level);
}

// Determines whether a draw image requires mips.
bool ShouldGenerateMips(const DrawImage& draw_image,
                        int upload_scale_mip_level) {
  // If filter quality is less than medium, don't generate mips.
  if (draw_image.filter_quality() < kMedium_SkFilterQuality)
    return false;

  gfx::Size base_size(draw_image.paint_image().width(),
                      draw_image.paint_image().height());
  // Take the abs of the scale, as mipmap functions don't handle (and aren't
  // impacted by) negative image dimensions.
  gfx::SizeF scaled_size = gfx::ScaleSize(
      gfx::SizeF(base_size), std::abs(draw_image.scale().width()),
      std::abs(draw_image.scale().height()));

  // If our target size is smaller than our scaled size in both dimension, we
  // need to generate mips.
  gfx::SizeF target_size =
      gfx::SizeF(CalculateSizeForMipLevel(draw_image, upload_scale_mip_level));
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
bool DrawAndScaleImage(const DrawImage& draw_image,
                       SkPixmap* target_pixmap,
                       PaintImage::GeneratorClientId client_id) {
  // We will pass color_space explicitly to PaintImage::Decode, so pull it out
  // of the pixmap and populate a stand-alone value.
  // note: To pull colorspace out of the pixmap, we create a new pixmap with
  // null colorspace but the same memory pointer.
  SkPixmap pixmap(target_pixmap->info().makeColorSpace(nullptr),
                  target_pixmap->writable_addr(), target_pixmap->rowBytes());
  sk_sp<SkColorSpace> color_space = target_pixmap->info().refColorSpace();

  const PaintImage& paint_image = draw_image.paint_image();
  const bool is_original_decode =
      SkISize::Make(paint_image.width(), paint_image.height()) ==
      pixmap.bounds().size();
  const bool is_nearest_neighbor =
      draw_image.filter_quality() == kNone_SkFilterQuality;

  SkISize supported_size =
      paint_image.GetSupportedDecodeSize(pixmap.bounds().size());
  // We can directly decode into target pixmap if we are doing an original
  // decode or we are decoding to scale without nearest neighbor filtering.
  const bool can_directly_decode = is_original_decode || !is_nearest_neighbor;
  if (supported_size == pixmap.bounds().size() && can_directly_decode) {
    SkImageInfo info = pixmap.info();
    return paint_image.Decode(pixmap.writable_addr(), &info, color_space,
                              draw_image.frame_index(), client_id);
  }

  // If we can't decode/scale directly, we will handle this in 2 steps.
  // Step 1: Decode at the nearest (larger) directly supported size or the
  // original size if nearest neighbor quality is requested.
  // Step 2: Scale to |pixmap| size. If decoded image is half float backed and
  // the device does not support image resize, decode to N32 color type and
  // convert to F16 afterward.
  SkISize decode_size =
      is_nearest_neighbor
          ? SkISize::Make(paint_image.width(), paint_image.height())
          : supported_size;
  SkImageInfo decode_info =
      pixmap.info().makeWH(decode_size.width(), decode_size.height());
  SkFilterQuality filter_quality = CalculateDesiredFilterQuality(draw_image);
  bool decode_to_f16_using_n32_intermediate =
      decode_info.colorType() == kRGBA_F16_SkColorType &&
      !ImageDecodeCacheUtils::CanResizeF16Image(filter_quality);
  if (decode_to_f16_using_n32_intermediate)
    decode_info = decode_info.makeColorType(kN32_SkColorType);

  SkBitmap decode_bitmap;
  if (!decode_bitmap.tryAllocPixels(decode_info))
    return false;
  SkPixmap decode_pixmap = decode_bitmap.pixmap();
  if (!paint_image.Decode(decode_pixmap.writable_addr(), &decode_info,
                          color_space, draw_image.frame_index(), client_id)) {
    return false;
  }

  if (decode_to_f16_using_n32_intermediate) {
    return ImageDecodeCacheUtils::ScaleToHalfFloatPixmapUsingN32Intermediate(
        decode_pixmap, &pixmap, filter_quality);
  }
  return decode_pixmap.scalePixels(pixmap, filter_quality);
}

// Takes ownership of the backing texture of an SkImage. This allows us to
// delete this texture under Skia (via discardable).
sk_sp<SkImage> TakeOwnershipOfSkImageBacking(GrContext* context,
                                             sk_sp<SkImage> image) {
  // If the image is not texture backed, it has no backing, just return it.
  if (!image->isTextureBacked()) {
    return image;
  }

  GrSurfaceOrigin origin;
  image->getBackendTexture(false /* flushPendingGrContextIO */, &origin);
  SkColorType color_type = image->colorType();
  if (color_type == kUnknown_SkColorType) {
    return nullptr;
  }
  sk_sp<SkColorSpace> color_space = image->refColorSpace();
  GrBackendTexture backend_texture;
  SkImage::BackendTextureReleaseProc release_proc;
  SkImage::MakeBackendTextureFromSkImage(context, std::move(image),
                                         &backend_texture, &release_proc);
  return SkImage::MakeFromTexture(context, backend_texture, origin, color_type,
                                  kPremul_SkAlphaType, std::move(color_space));
}

// Immediately deletes an SkImage, preventing caching of that image. Must be
// called while holding the context lock.
void DeleteSkImageAndPreventCaching(viz::RasterContextProvider* context,
                                    sk_sp<SkImage>&& image) {
  // No need to do anything for a non-texture-backed images.
  if (!image->isTextureBacked())
    return;

  sk_sp<SkImage> image_owned =
      TakeOwnershipOfSkImageBacking(context->GrContext(), std::move(image));
  // If context is lost, we may get a null image here.
  if (image_owned) {
    // Delete |original_image_owned| as Skia will not clean it up. We are
    // holding the context lock here, so we can delete immediately.
    uint32_t texture_id =
        GpuImageDecodeCache::GlIdFromSkImage(image_owned.get());
    context->ContextGL()->DeleteTextures(1, &texture_id);
  }
}

// TODO(ericrk): Replace calls to this with calls to SkImage::makeTextureImage,
// once that function handles colorspaces. https://crbug.com/834837
sk_sp<SkImage> MakeTextureImage(viz::RasterContextProvider* context,
                                sk_sp<SkImage> source_image,
                                sk_sp<SkColorSpace> target_color_space,
                                GrMipMapped mip_mapped) {
  // Step 1: Upload image and generate mips if necessary. If we will be applying
  // a color-space conversion, don't generate mips yet, instead do it after
  // conversion, in step 3.
  bool add_mips_after_color_conversion =
      (target_color_space && mip_mapped == GrMipMapped::kYes);
  sk_sp<SkImage> uploaded_image = source_image->makeTextureImage(
      context->GrContext(), nullptr,
      add_mips_after_color_conversion ? GrMipMapped::kNo : mip_mapped);

  // Step 2: Apply a color-space conversion if necessary.
  if (uploaded_image && target_color_space) {
    sk_sp<SkImage> pre_converted_image = uploaded_image;
    uploaded_image = uploaded_image->makeColorSpace(target_color_space);

    if (uploaded_image != pre_converted_image)
      DeleteSkImageAndPreventCaching(context, std::move(pre_converted_image));
  }

  // Step 3: If we had a colorspace conversion, we couldn't mipmap in step 1, so
  // add mips here.
  if (uploaded_image && add_mips_after_color_conversion) {
    sk_sp<SkImage> pre_mipped_image = uploaded_image;
    uploaded_image = uploaded_image->makeTextureImage(
        context->GrContext(), nullptr, GrMipMapped::kYes);
    DCHECK_NE(pre_mipped_image, uploaded_image);
    DeleteSkImageAndPreventCaching(context, std::move(pre_mipped_image));
  }

  return uploaded_image;
}

}  // namespace

// static
GpuImageDecodeCache::InUseCacheKey
GpuImageDecodeCache::InUseCacheKey::FromDrawImage(const DrawImage& draw_image) {
  return InUseCacheKey(draw_image);
}

// Extract the information to uniquely identify a DrawImage for the purposes of
// the |in_use_cache_|.
GpuImageDecodeCache::InUseCacheKey::InUseCacheKey(const DrawImage& draw_image)
    : frame_key(draw_image.frame_key()),
      upload_scale_mip_level(CalculateUploadScaleMipLevel(draw_image)),
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
      base::HashInts(cache_key.frame_key.hash(),
                     base::HashInts(cache_key.upload_scale_mip_level,
                                    cache_key.filter_quality)));
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
                         GpuImageDecodeCache::DecodeTaskType task_type)
      : TileTask(true),
        cache_(cache),
        image_(draw_image),
        tracing_info_(tracing_info),
        task_type_(task_type) {
    DCHECK(!SkipImage(draw_image));
  }

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "GpuImageDecodeTaskImpl::RunOnWorkerThread", "mode",
                 "gpu", "source_prepare_tiles_id",
                 tracing_info_.prepare_tiles_id);
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        &image_.paint_image(),
        devtools_instrumentation::ScopedImageDecodeTask::kGpu,
        ImageDecodeCache::ToScopedTaskType(tracing_info_.task_type));
    cache_->DecodeImageInTask(image_, tracing_info_.task_type);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    cache_->OnImageDecodeTaskCompleted(image_, task_type_);
  }

 protected:
  ~GpuImageDecodeTaskImpl() override = default;

 private:
  GpuImageDecodeCache* cache_;
  DrawImage image_;
  const ImageDecodeCache::TracingInfo tracing_info_;
  const GpuImageDecodeCache::DecodeTaskType task_type_;

  DISALLOW_COPY_AND_ASSIGN(GpuImageDecodeTaskImpl);
};

// Task which creates an image from decoded data. Typically this involves
// uploading data to the GPU, which requires this task be run on the non-
// concurrent thread.
class ImageUploadTaskImpl : public TileTask {
 public:
  ImageUploadTaskImpl(GpuImageDecodeCache* cache,
                      const DrawImage& draw_image,
                      scoped_refptr<TileTask> decode_dependency,
                      const ImageDecodeCache::TracingInfo& tracing_info)
      : TileTask(false),
        cache_(cache),
        image_(draw_image),
        tracing_info_(tracing_info) {
    DCHECK(!SkipImage(draw_image));
    // If an image is already decoded and locked, we will not generate a
    // decode task.
    if (decode_dependency)
      dependencies_.push_back(std::move(decode_dependency));
  }

  // Override from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageUploadTaskImpl::RunOnWorkerThread", "mode", "gpu",
                 "source_prepare_tiles_id", tracing_info_.prepare_tiles_id);
    cache_->UploadImageInTask(image_);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    cache_->OnImageUploadTaskCompleted(image_);
  }

 protected:
  ~ImageUploadTaskImpl() override = default;

 private:
  GpuImageDecodeCache* cache_;
  DrawImage image_;
  const ImageDecodeCache::TracingInfo tracing_info_;

  DISALLOW_COPY_AND_ASSIGN(ImageUploadTaskImpl);
};

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

GpuImageDecodeCache::DecodedImageData::DecodedImageData(bool is_bitmap_backed)
    : is_bitmap_backed_(is_bitmap_backed) {}
GpuImageDecodeCache::DecodedImageData::~DecodedImageData() {
  ResetData();
}

bool GpuImageDecodeCache::DecodedImageData::Lock() {
  if (data_->Lock())
    OnLock();
  return is_locked_;
}

void GpuImageDecodeCache::DecodedImageData::Unlock() {
  data_->Unlock();
  OnUnlock();
}

void GpuImageDecodeCache::DecodedImageData::SetLockedData(
    std::unique_ptr<base::DiscardableMemory> data,
    sk_sp<SkImage> image,
    bool out_of_raster) {
  DCHECK(data);
  DCHECK(!data_);
  DCHECK(image);
  DCHECK(!image_);
  data_ = std::move(data);
  image_ = std::move(image);
  OnSetLockedData(out_of_raster);
}

void GpuImageDecodeCache::DecodedImageData::SetBitmapImage(
    sk_sp<SkImage> image) {
  DCHECK(is_bitmap_backed_);
  image_ = std::move(image);
  OnLock();
}

void GpuImageDecodeCache::DecodedImageData::ResetBitmapImage() {
  DCHECK(is_bitmap_backed_);
  image_ = nullptr;
  OnUnlock();
}

void GpuImageDecodeCache::DecodedImageData::ResetData() {
  if (data_) {
    DCHECK(image_);
    ReportUsageStats();
  }
  image_ = nullptr;
  data_ = nullptr;
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

GpuImageDecodeCache::UploadedImageData::UploadedImageData() = default;
GpuImageDecodeCache::UploadedImageData::~UploadedImageData() {
  DCHECK(!image());
}

void GpuImageDecodeCache::UploadedImageData::SetImage(sk_sp<SkImage> image) {
  DCHECK(mode_ == Mode::kNone);
  DCHECK(!image_);
  DCHECK(!transfer_cache_id_);
  DCHECK(image);

  mode_ = Mode::kSkImage;
  image_ = std::move(image);
  if (image_->isTextureBacked())
    gl_id_ = GlIdFromSkImage(image_.get());
  OnSetLockedData(false /* out_of_raster */);
}

void GpuImageDecodeCache::UploadedImageData::SetTransferCacheId(uint32_t id) {
  DCHECK(mode_ == Mode::kNone);
  DCHECK(!image_);
  DCHECK(!transfer_cache_id_);

  mode_ = Mode::kTransferCache;
  transfer_cache_id_ = id;
  OnSetLockedData(false /* out_of_raster */);
}

void GpuImageDecodeCache::UploadedImageData::Reset() {
  if (mode_ != Mode::kNone)
    ReportUsageStats();

  mode_ = Mode::kNone;
  image_ = nullptr;
  gl_id_ = 0;
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

GpuImageDecodeCache::ImageData::ImageData(
    PaintImage::Id paint_image_id,
    DecodedDataMode mode,
    size_t size,
    const gfx::ColorSpace& target_color_space,
    SkFilterQuality quality,
    int upload_scale_mip_level,
    bool needs_mips,
    bool is_bitmap_backed)
    : paint_image_id(paint_image_id),
      mode(mode),
      size(size),
      target_color_space(target_color_space),
      quality(quality),
      upload_scale_mip_level(upload_scale_mip_level),
      needs_mips(needs_mips),
      is_bitmap_backed(is_bitmap_backed),
      decode(is_bitmap_backed) {}

GpuImageDecodeCache::ImageData::~ImageData() {
  // We should never delete ImageData while it is in use or before it has been
  // cleaned up.
  DCHECK_EQ(0u, upload.ref_count);
  DCHECK_EQ(0u, decode.ref_count);
  DCHECK_EQ(false, decode.is_locked());
  // This should always be cleaned up before deleting the image, as it needs to
  // be freed with the GL context lock held.
  DCHECK(!HasUploadedData());
}

bool GpuImageDecodeCache::ImageData::IsGpuOrTransferCache() const {
  return mode == DecodedDataMode::kGpu ||
         mode == DecodedDataMode::kTransferCache;
}

bool GpuImageDecodeCache::ImageData::HasUploadedData() const {
  switch (mode) {
    case DecodedDataMode::kGpu:
      return !!upload.image();
    case DecodedDataMode::kTransferCache:
      return !!upload.transfer_cache_id();
    case DecodedDataMode::kCpu:
      return false;
  }
  return false;
}

void GpuImageDecodeCache::ImageData::ValidateBudgeted() const {
  // If the image is budgeted, it must be refed.
  DCHECK(is_budgeted);
  DCHECK_GT(upload.ref_count, 0u);
}

// static
GrGLuint GpuImageDecodeCache::GlIdFromSkImage(const SkImage* image) {
  DCHECK(image->isTextureBacked());
  GrBackendTexture backend_texture =
      image->getBackendTexture(true /* flushPendingGrContextIO */);
  if (!backend_texture.isValid())
    return 0;

  GrGLTextureInfo info;
  if (!backend_texture.getGLTextureInfo(&info))
    return 0;

  return info.fID;
}

GpuImageDecodeCache::GpuImageDecodeCache(
    viz::RasterContextProvider* context,
    bool use_transfer_cache,
    SkColorType color_type,
    size_t max_working_set_bytes,
    int max_texture_size,
    PaintImage::GeneratorClientId generator_client_id)
    : color_type_(color_type),
      use_transfer_cache_(use_transfer_cache),
      context_(context),
      max_texture_size_(max_texture_size),
      generator_client_id_(generator_client_id),
      persistent_cache_(PersistentCache::NO_AUTO_EVICT),
      max_working_set_bytes_(max_working_set_bytes),
      max_working_set_items_(kMaxItemsInWorkingSet) {
  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::GpuImageDecodeCache", base::ThreadTaskRunnerHandle::Get());
  }
  memory_pressure_listener_.reset(
      new base::MemoryPressureListener(base::BindRepeating(
          &GpuImageDecodeCache::OnMemoryPressure, base::Unretained(this))));
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

  // TODO(vmpstr): If we don't have a client name, it may cause problems in
  // unittests, since most tests don't set the name but some do. The UMA system
  // expects the name to be always the same. This assertion is violated in the
  // tests that do set the name.
  if (GetClientNameForMetrics()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        base::StringPrintf("Compositing.%s.CachedImagesCount.Gpu",
                           GetClientNameForMetrics()),
        lifetime_max_items_in_cache_, 1, 1000, 20);
  }
}

ImageDecodeCache::TaskResult GpuImageDecodeCache::GetTaskForImageAndRef(
    const DrawImage& draw_image,
    const TracingInfo& tracing_info) {
  DCHECK_EQ(tracing_info.task_type, TaskType::kInRaster);
  return GetTaskForImageAndRefInternal(draw_image, tracing_info,
                                       DecodeTaskType::kPartOfUploadTask);
}

ImageDecodeCache::TaskResult
GpuImageDecodeCache::GetOutOfRasterDecodeTaskForImageAndRef(
    const DrawImage& draw_image) {
  return GetTaskForImageAndRefInternal(
      draw_image, TracingInfo(0, TilePriority::NOW, TaskType::kOutOfRaster),
      DecodeTaskType::kStandAloneDecodeTask);
}

ImageDecodeCache::TaskResult GpuImageDecodeCache::GetTaskForImageAndRefInternal(
    const DrawImage& draw_image,
    const TracingInfo& tracing_info,
    DecodeTaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetTaskForImageAndRef");

  if (SkipImage(draw_image))
    return TaskResult(false);

  base::AutoLock lock(lock_);
  const InUseCacheKey cache_key = InUseCacheKey::FromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  scoped_refptr<ImageData> new_data;
  if (!image_data) {
    // We need an ImageData, create one now.
    new_data = CreateImageData(draw_image);
    image_data = new_data.get();
  } else if (image_data->decode.decode_failure) {
    // We have already tried and failed to decode this image, so just return.
    return TaskResult(false);
  } else if (task_type == DecodeTaskType::kPartOfUploadTask &&
             image_data->upload.task) {
    // We had an existing upload task, ref the image and return the task.
    image_data->ValidateBudgeted();
    RefImage(draw_image, cache_key);
    return TaskResult(image_data->upload.task);
  } else if (task_type == DecodeTaskType::kStandAloneDecodeTask &&
             image_data->decode.stand_alone_task) {
    // We had an existing out of raster task, ref the image and return the task.
    image_data->ValidateBudgeted();
    RefImage(draw_image, cache_key);
    return TaskResult(image_data->decode.stand_alone_task);
  }

  // Ensure that the image we're about to decode/upload will fit in memory, if
  // not already budgeted.
  if (!image_data->is_budgeted && !EnsureCapacity(image_data->size)) {
    // Image will not fit, do an at-raster decode.
    return TaskResult(false);
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
    return TaskResult(true);
  }

  scoped_refptr<TileTask> task;
  if (task_type == DecodeTaskType::kPartOfUploadTask) {
    // Ref image and create a upload and decode tasks. We will release this ref
    // in UploadTaskCompleted.
    RefImage(draw_image, cache_key);
    task = base::MakeRefCounted<ImageUploadTaskImpl>(
        this, draw_image,
        GetImageDecodeTaskAndRef(draw_image, tracing_info, task_type),
        tracing_info);
    image_data->upload.task = task;
  } else {
    task = GetImageDecodeTaskAndRef(draw_image, tracing_info, task_type);
  }

  return TaskResult(task);
}

void GpuImageDecodeCache::UnrefImage(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UnrefImage");
  base::AutoLock lock(lock_);
  UnrefImageInternal(draw_image, InUseCacheKey::FromDrawImage(draw_image));
}

bool GpuImageDecodeCache::UseCacheForDrawImage(
    const DrawImage& draw_image) const {
  if (draw_image.paint_image().GetSkImage()->isTextureBacked())
    return false;

  return true;
}

DecodedDrawImage GpuImageDecodeCache::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  TRACE_EVENT0("cc", "GpuImageDecodeCache::GetDecodedImageForDraw");

  // We are being called during raster. The context lock must already be
  // acquired by the caller.
  CheckContextLockAcquiredIfNecessary();

  // If we're skipping the image, then the filter quality doesn't matter.
  if (SkipImage(draw_image))
    return DecodedDrawImage();

  base::AutoLock lock(lock_);
  const InUseCacheKey cache_key = InUseCacheKey::FromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  if (!image_data) {
    // We didn't find the image, create a new entry.
    auto data = CreateImageData(draw_image);
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
  DecodeImageIfNecessary(draw_image, image_data, TaskType::kInRaster);
  UploadImageIfNecessary(draw_image, image_data);
  // Unref the image decode, but not the image. The image ref will be released
  // in DrawWithImageFinished.
  UnrefImageDecode(draw_image, cache_key);

  if (image_data->mode == DecodedDataMode::kTransferCache) {
    DCHECK(use_transfer_cache_);
    auto id = image_data->upload.transfer_cache_id();
    if (id)
      image_data->upload.mark_used();
    DCHECK(id || image_data->decode.decode_failure);

    SkSize scale_factor = CalculateScaleFactorForMipLevel(
        draw_image, image_data->upload_scale_mip_level);
    DecodedDrawImage decoded_draw_image(
        id, SkSize(), scale_factor, CalculateDesiredFilterQuality(draw_image),
        image_data->needs_mips, image_data->is_budgeted);
    return decoded_draw_image;
  } else {
    DCHECK(!use_transfer_cache_);
    sk_sp<SkImage> image = image_data->upload.image();
    if (image)
      image_data->upload.mark_used();
    DCHECK(image || image_data->decode.decode_failure);

    SkSize scale_factor = CalculateScaleFactorForMipLevel(
        draw_image, image_data->upload_scale_mip_level);
    DecodedDrawImage decoded_draw_image(
        std::move(image), SkSize(), scale_factor,
        CalculateDesiredFilterQuality(draw_image), image_data->is_budgeted);
    return decoded_draw_image;
  }
}

void GpuImageDecodeCache::DrawWithImageFinished(
    const DrawImage& draw_image,
    const DecodedDrawImage& decoded_draw_image) {
  TRACE_EVENT0("cc", "GpuImageDecodeCache::DrawWithImageFinished");

  // Release decoded_draw_image to ensure the referenced SkImage can be
  // cleaned up below.
  { auto delete_decoded_draw_image = std::move(decoded_draw_image); }

  // We are being called during raster. The context lock must already be
  // acquired by the caller.
  CheckContextLockAcquiredIfNecessary();

  if (SkipImage(draw_image))
    return;

  base::AutoLock lock(lock_);
  UnrefImageInternal(draw_image, InUseCacheKey::FromDrawImage(draw_image));

  // We are mid-draw and holding the context lock, ensure we clean up any
  // textures (especially at-raster), which may have just been marked for
  // deletion by UnrefImage.
  RunPendingContextThreadOperations();
}

void GpuImageDecodeCache::ReduceCacheUsage() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::ReduceCacheUsage");
  base::AutoLock lock(lock_);
  EnsureCapacity(0);

  // This is typically called when no tasks are running (between scheduling
  // tasks). Try to lock and run pending operations if possible, but don't
  // block on it.
  if (context_->GetLock() && !context_->GetLock()->Try())
    return;

  RunPendingContextThreadOperations();
  if (context_->GetLock())
    context_->GetLock()->Release();
}

void GpuImageDecodeCache::SetShouldAggressivelyFreeResources(
    bool aggressively_free_resources) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::SetShouldAggressivelyFreeResources",
               "agressive_free_resources", aggressively_free_resources);
  if (aggressively_free_resources) {
    base::Optional<viz::RasterContextProvider::ScopedRasterContextLock>
        context_lock;
    if (context_->GetLock())
      context_lock.emplace(context_);

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
}

void GpuImageDecodeCache::AddToPersistentCache(const DrawImage& draw_image,
                                               scoped_refptr<ImageData> data) {
  lock_.AssertAcquired();

  WillAddCacheEntry(draw_image);
  persistent_cache_.Put(draw_image.frame_key(), std::move(data));
}

template <typename Iterator>
Iterator GpuImageDecodeCache::RemoveFromPersistentCache(Iterator it) {
  lock_.AssertAcquired();

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
  DCHECK(entries_it != paint_image_entries_.end());
  DCHECK_GT(entries_it->second.count, 0u);

  // If this is the last entry for this image, remove its tracking.
  --entries_it->second.count;
  if (entries_it->second.count == 0u)
    paint_image_entries_.erase(entries_it);

  return persistent_cache_.Erase(it);
}

size_t GpuImageDecodeCache::GetMaximumMemoryLimitBytes() const {
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

  if (args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name = base::StringPrintf(
        "cc/image_memory/cache_0x%" PRIXPTR, reinterpret_cast<uintptr_t>(this));
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
    if (image_data->decode.data()) {
      std::string discardable_dump_name = base::StringPrintf(
          "cc/image_memory/cache_0x%" PRIXPTR "/discardable/image_%d",
          reinterpret_cast<uintptr_t>(this), image_id);
      MemoryAllocatorDump* dump =
          image_data->decode.data()->CreateMemoryAllocatorDump(
              discardable_dump_name.c_str(), pmd);
      // Dump the "locked_size" as an additional column.
      // This lets us see the amount of discardable which is contributing to
      // memory pressure.
      size_t locked_size =
          image_data->decode.is_locked() ? image_data->size : 0u;
      dump->AddScalar("locked_size", MemoryAllocatorDump::kUnitsBytes,
                      locked_size);
    }

    // If we have an uploaded image (that is actually on the GPU, not just a
    // CPU wrapper), upload it here.
    if (image_data->HasUploadedData() &&
        image_data->mode == DecodedDataMode::kGpu) {
      size_t discardable_size = image_data->size;
      // If the discardable system has deleted this out from under us, log a
      // size of 0 to match software discardable.
      if (context_->ContextSupport()
              ->ThreadsafeDiscardableTextureIsDeletedForTracing(
                  image_data->upload.gl_id())) {
        discardable_size = 0;
      }

      std::string gpu_dump_name = base::StringPrintf(
          "cc/image_memory/cache_0x%" PRIXPTR "/gpu/image_%d",
          reinterpret_cast<uintptr_t>(this), image_id);
      MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(gpu_dump_name);
      dump->AddScalar(MemoryAllocatorDump::kNameSize,
                      MemoryAllocatorDump::kUnitsBytes, discardable_size);

      // Dump the "locked_size" as an additional column.
      size_t locked_size =
          image_data->upload.is_locked() ? discardable_size : 0u;
      dump->AddScalar("locked_size", MemoryAllocatorDump::kUnitsBytes,
                      locked_size);

      // Create a global shred GUID to associate this data with its GPU
      // process counterpart.
      MemoryAllocatorDumpGuid guid = gl::GetGLTextureClientGUIDForTracing(
          context_->ContextSupport()->ShareGroupTracingGUID(),
          image_data->upload.gl_id());

      // kImportance is somewhat arbitrary - we chose 3 to be higher than the
      // value used in the GPU process (1), and Skia (2), causing us to appear
      // as the owner in memory traces.
      const int kImportance = 3;
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
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
      draw_image, InUseCacheKey::FromDrawImage(draw_image));
  DCHECK(image_data);
  DCHECK(image_data->is_budgeted) << "Must budget an image for pre-decoding";
  DecodeImageIfNecessary(draw_image, image_data, task_type);
}

void GpuImageDecodeCache::UploadImageInTask(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UploadImage");
  base::Optional<viz::RasterContextProvider::ScopedRasterContextLock>
      context_lock;
  if (context_->GetLock())
    context_lock.emplace(context_);

  base::Optional<ScopedGrContextAccess> gr_context_access;
  if (!use_transfer_cache_)
    gr_context_access.emplace(context_);
  base::AutoLock lock(lock_);

  auto cache_key = InUseCacheKey::FromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  DCHECK(image_data);
  DCHECK(image_data->is_budgeted) << "Must budget an image for pre-decoding";

  if (image_data->is_bitmap_backed)
    DecodeImageIfNecessary(draw_image, image_data, TaskType::kInRaster);
  UploadImageIfNecessary(draw_image, image_data);
}

void GpuImageDecodeCache::OnImageDecodeTaskCompleted(
    const DrawImage& draw_image,
    DecodeTaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::OnImageDecodeTaskCompleted");
  base::AutoLock lock(lock_);
  auto cache_key = InUseCacheKey::FromDrawImage(draw_image);
  // Decode task is complete, remove our reference to it.
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  DCHECK(image_data);
  if (task_type == DecodeTaskType::kPartOfUploadTask) {
    DCHECK(image_data->decode.task);
    image_data->decode.task = nullptr;
  } else {
    DCHECK(task_type == DecodeTaskType::kStandAloneDecodeTask);
    DCHECK(image_data->decode.stand_alone_task);
    image_data->decode.stand_alone_task = nullptr;
  }

  // While the decode task is active, we keep a ref on the decoded data.
  // Release that ref now.
  UnrefImageDecode(draw_image, cache_key);
}

void GpuImageDecodeCache::OnImageUploadTaskCompleted(
    const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::OnImageUploadTaskCompleted");
  base::AutoLock lock(lock_);
  // Upload task is complete, remove our reference to it.
  InUseCacheKey cache_key = InUseCacheKey::FromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  DCHECK(image_data);
  DCHECK(image_data->upload.task);
  image_data->upload.task = nullptr;

  // While the upload task is active, we keep a ref on both the image it will be
  // populating, as well as the decode it needs to populate it. Release these
  // refs now.
  UnrefImageDecode(draw_image, cache_key);
  UnrefImageInternal(draw_image, cache_key);
}

// Checks if an existing image decode exists. If not, returns a task to produce
// the requested decode.
scoped_refptr<TileTask> GpuImageDecodeCache::GetImageDecodeTaskAndRef(
    const DrawImage& draw_image,
    const TracingInfo& tracing_info,
    DecodeTaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetImageDecodeTaskAndRef");
  lock_.AssertAcquired();

  auto cache_key = InUseCacheKey::FromDrawImage(draw_image);

  // This ref is kept alive while an upload task may need this decode. We
  // release this ref in UploadTaskCompleted.
  if (task_type == DecodeTaskType::kPartOfUploadTask)
    RefImageDecode(draw_image, cache_key);

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
  scoped_refptr<TileTask>& existing_task =
      (task_type == DecodeTaskType::kPartOfUploadTask)
          ? image_data->decode.task
          : image_data->decode.stand_alone_task;
  if (!existing_task) {
    // Ref image decode and create a decode task. This ref will be released in
    // DecodeTaskCompleted.
    RefImageDecode(draw_image, cache_key);
    existing_task = base::MakeRefCounted<GpuImageDecodeTaskImpl>(
        this, draw_image, tracing_info, task_type);
  }
  return existing_task;
}

void GpuImageDecodeCache::RefImageDecode(const DrawImage& draw_image,
                                         const InUseCacheKey& cache_key) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::RefImageDecode");
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(cache_key);
  DCHECK(found != in_use_cache_.end());
  ++found->second.ref_count;
  ++found->second.image_data->decode.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
}

void GpuImageDecodeCache::UnrefImageDecode(const DrawImage& draw_image,
                                           const InUseCacheKey& cache_key) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UnrefImageDecode");
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(cache_key);
  DCHECK(found != in_use_cache_.end());
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
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(cache_key);

  // If no secondary cache entry was found for the given |draw_image|, then
  // the draw_image only exists in the |persistent_cache_|. Create an in-use
  // cache entry now.
  if (found == in_use_cache_.end()) {
    auto found_image = persistent_cache_.Peek(draw_image.frame_key());
    DCHECK(found_image != persistent_cache_.end());
    DCHECK(IsCompatible(found_image->second.get(), draw_image));
    found = in_use_cache_
                .insert(InUseCache::value_type(
                    cache_key, InUseCacheEntry(found_image->second)))
                .first;
  }

  DCHECK(found != in_use_cache_.end());
  ++found->second.ref_count;
  ++found->second.image_data->upload.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
}

void GpuImageDecodeCache::UnrefImageInternal(const DrawImage& draw_image,
                                             const InUseCacheKey& cache_key) {
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(cache_key);
  DCHECK(found != in_use_cache_.end());
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
  lock_.AssertAcquired();

  bool has_any_refs =
      image_data->upload.ref_count > 0 || image_data->decode.ref_count > 0;
  // If we have no image refs on an image, we should unbudget it.
  if (!has_any_refs && image_data->is_budgeted) {
    DCHECK_GE(working_set_bytes_, image_data->size);
    DCHECK_GE(working_set_items_, 1u);
    working_set_bytes_ -= image_data->size;
    working_set_items_ -= 1;
    image_data->is_budgeted = false;
  }

  // Don't keep around completely empty images. This can happen if an image's
  // decode/upload tasks were both cancelled before completing.
  const bool has_cpu_data =
      image_data->decode.data() ||
      (image_data->is_bitmap_backed && image_data->decode.image());
  if (!has_any_refs && !image_data->HasUploadedData() && !has_cpu_data &&
      !image_data->is_orphaned) {
    auto found_persistent = persistent_cache_.Peek(draw_image.frame_key());
    if (found_persistent != persistent_cache_.end())
      RemoveFromPersistentCache(found_persistent);
  }

  // If we have no refs on an uploaded image, it should be unlocked. Do this
  // before any attempts to delete the image.
  if (image_data->IsGpuOrTransferCache() && image_data->upload.ref_count == 0 &&
      image_data->upload.is_locked()) {
    UnlockImage(image_data);
  }

  // Don't keep around orphaned images.
  if (image_data->is_orphaned && !has_any_refs) {
    DeleteImage(image_data);
  }

  // Don't keep CPU images if they are unused, these images can be recreated by
  // re-locking discardable (rather than requiring a full upload like GPU
  // images).
  if (image_data->mode == DecodedDataMode::kCpu && !has_any_refs) {
    DeleteImage(image_data);
  }

  // If we have image that could be budgeted, but isn't, budget it now.
  if (has_any_refs && !image_data->is_budgeted &&
      CanFitInWorkingSet(image_data->size)) {
    working_set_bytes_ += image_data->size;
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
      DCHECK(!image_data->decode.data());
      image_data->decode.ResetBitmapImage();
    } else {
      DCHECK(image_data->decode.data());
      image_data->decode.Unlock();
    }
  }

  // EnsureCapacity to make sure we are under our cache limits.
  EnsureCapacity(0);

#if DCHECK_IS_ON()
  // Sanity check the above logic.
  if (image_data->HasUploadedData()) {
    if (image_data->mode == DecodedDataMode::kCpu)
      DCHECK(image_data->decode.is_locked());
  } else {
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
  lock_.AssertAcquired();

  lifetime_max_items_in_cache_ =
      std::max(lifetime_max_items_in_cache_, persistent_cache_.size());

  // While we are over preferred item capacity, we iterate through our set of
  // cached image data in LRU order, removing unreferenced images.
  for (auto it = persistent_cache_.rbegin();
       it != persistent_cache_.rend() && ExceedsPreferredCount();) {
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

bool GpuImageDecodeCache::ExceedsPreferredCount() const {
  lock_.AssertAcquired();

  size_t items_limit;
  if (aggressively_freeing_resources_) {
    items_limit = kSuspendedMaxItemsInCacheForGpu;
  } else {
    items_limit = kNormalMaxItemsInCacheForGpu;
  }

  return persistent_cache_.size() > items_limit;
}

void GpuImageDecodeCache::DecodeImageIfNecessary(const DrawImage& draw_image,
                                                 ImageData* image_data,
                                                 TaskType task_type) {
  lock_.AssertAcquired();

  DCHECK_GT(image_data->decode.ref_count, 0u);

  if (image_data->decode.decode_failure) {
    // We have already tried and failed to decode this image. Don't try again.
    return;
  }

  if (image_data->HasUploadedData() &&
      TryLockImage(HaveContextLock::kNo, draw_image, image_data)) {
    // We already have an uploaded image, no reason to decode.
    return;
  }

  if (image_data->is_bitmap_backed) {
    DCHECK(!draw_image.paint_image().IsLazyGenerated());
    image_data->decode.SetBitmapImage(draw_image.paint_image().GetSkImage());
    return;
  }

  if (image_data->decode.data() &&
      (image_data->decode.is_locked() || image_data->decode.Lock())) {
    // We already decoded this, or we just needed to lock, early out.
    return;
  }

  TRACE_EVENT0("cc", "GpuImageDecodeCache::DecodeImage");
  RecordImageMipLevelUMA(image_data->upload_scale_mip_level);

  image_data->decode.ResetData();
  std::unique_ptr<base::DiscardableMemory> backing_memory;
  sk_sp<SkImage> image;
  {
    base::AutoUnlock unlock(lock_);
    backing_memory = base::DiscardableMemoryAllocator::GetInstance()
                         ->AllocateLockedDiscardableMemory(image_data->size);
    SkImageInfo image_info = CreateImageInfoForDrawImage(
        draw_image, image_data->upload_scale_mip_level);
    SkPixmap pixmap(image_info, backing_memory->data(),
                    image_info.minRowBytes());

    // Set |pixmap| to the desired colorspace to decode into.
    pixmap.setColorSpace(
        ColorSpaceForImageDecode(draw_image, image_data->mode));
    if (!DrawAndScaleImage(draw_image, &pixmap, generator_client_id_)) {
      DLOG(ERROR) << "DrawAndScaleImage failed.";
      backing_memory->Unlock();
      backing_memory.reset();
    } else {
      image =
          SkImage::MakeFromRaster(pixmap, [](const void*, void*) {}, nullptr);
    }
  }

  if (image_data->decode.data()) {
    DCHECK(image_data->decode.image());
    // An at-raster task decoded this before us. Ingore our decode.
    return;
  }

  if (!backing_memory) {
    DCHECK(!image);
    // If |backing_memory| was not populated, we had a non-decodable image.
    image_data->decode.decode_failure = true;
    return;
  }

  image_data->decode.SetLockedData(std::move(backing_memory), std::move(image),
                                   task_type == TaskType::kOutOfRaster);
}

void GpuImageDecodeCache::UploadImageIfNecessary(const DrawImage& draw_image,
                                                 ImageData* image_data) {
  CheckContextLockAcquiredIfNecessary();
  lock_.AssertAcquired();

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
  UpdateMipsIfNeeded(draw_image, image_data);

  // If we have uploaded data at this point, it is locked with correct mips,
  // just return.
  if (image_data->HasUploadedData())
    return;

  TRACE_EVENT0("cc", "GpuImageDecodeCache::UploadImage");
  DCHECK(image_data->decode.is_locked());
  DCHECK_GT(image_data->decode.ref_count, 0u);
  DCHECK_GT(image_data->upload.ref_count, 0u);

  sk_sp<SkColorSpace> target_color_space =
      SupportsColorSpaceConversion() &&
              draw_image.target_color_space().IsValid()
          ? draw_image.target_color_space().ToSkColorSpace()
          : nullptr;
  if (target_color_space &&
      SkColorSpace::Equals(target_color_space.get(),
                           image_data->decode.image()->colorSpace())) {
    target_color_space = nullptr;
  }

  if (image_data->mode == DecodedDataMode::kTransferCache) {
    DCHECK(use_transfer_cache_);
    SkPixmap pixmap;
    if (!image_data->decode.image()->peekPixels(&pixmap))
      return;

    ClientImageTransferCacheEntry image_entry(&pixmap, target_color_space.get(),
                                              image_data->needs_mips);
    size_t size = image_entry.SerializedSize();
    void* data = context_->ContextSupport()->MapTransferCacheEntry(size);
    if (data) {
      bool succeeded = image_entry.Serialize(
          base::make_span(reinterpret_cast<uint8_t*>(data), size));
      DCHECK(succeeded);
      context_->ContextSupport()->UnmapAndCreateTransferCacheEntry(
          image_entry.UnsafeType(), image_entry.Id());
      image_data->upload.SetTransferCacheId(image_entry.Id());
    } else {
      // Transfer cache entry can fail due to a lost gpu context or failure
      // to allocate shared memory.  Handle this gracefully.  Mark this
      // image as "decode failed" so that we do not try to handle it again.
      // If this was a lost context, we'll recreate this image decode cache.
      image_data->decode.decode_failure = true;
    }

    return;
  }

  // If we reached this point, we are in the CPU/GPU path (not transfer cache).
  DCHECK(!use_transfer_cache_);

  // Grab a reference to our decoded image. For the kCpu path, we will use this
  // directly as our "uploaded" data.
  sk_sp<SkImage> uploaded_image = image_data->decode.image();
  image_data->decode.mark_used();

  // For kGpu, we upload and color convert (if necessary).
  if (image_data->mode == DecodedDataMode::kGpu) {
    DCHECK(!use_transfer_cache_);
    base::AutoUnlock unlock(lock_);
    uploaded_image = MakeTextureImage(
        context_, std::move(uploaded_image), target_color_space,
        image_data->needs_mips ? GrMipMapped::kYes : GrMipMapped::kNo);
  }

  // At-raster may have decoded this while we were unlocked. If so, ignore our
  // result.
  if (image_data->upload.image()) {
    if (uploaded_image)
      DeleteSkImageAndPreventCaching(context_, std::move(uploaded_image));
    return;
  }

  // Take ownership of any GL texture backing for the SkImage. This allows
  // us to use the image with the discardable system.
  if (uploaded_image) {
    uploaded_image = TakeOwnershipOfSkImageBacking(context_->GrContext(),
                                                   std::move(uploaded_image));
  }

  // TODO(crbug.com/740737): uploaded_image is sometimes null in certain
  // context-lost situations.
  if (!uploaded_image)
    return;

  image_data->upload.SetImage(std::move(uploaded_image));

  // If we have a new GPU-backed image, initialize it for use in the GPU
  // discardable system.
  if (image_data->mode == DecodedDataMode::kGpu) {
    // Notify the discardable system of this image so it will count against
    // budgets.
    context_->ContextGL()->InitializeDiscardableTextureCHROMIUM(
        image_data->upload.gl_id());
  }
}

scoped_refptr<GpuImageDecodeCache::ImageData>
GpuImageDecodeCache::CreateImageData(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::CreateImageData");
  lock_.AssertAcquired();

  int upload_scale_mip_level = CalculateUploadScaleMipLevel(draw_image);
  bool needs_mips = ShouldGenerateMips(draw_image, upload_scale_mip_level);
  SkImageInfo image_info =
      CreateImageInfoForDrawImage(draw_image, upload_scale_mip_level);

  DecodedDataMode mode;
  if (use_transfer_cache_) {
    mode = DecodedDataMode::kTransferCache;
  } else if (image_info.width() > max_texture_size_ ||
             image_info.height() > max_texture_size_) {
    // Image too large to upload. Try to use SW fallback.
    mode = DecodedDataMode::kCpu;
  } else {
    mode = DecodedDataMode::kGpu;
  }

  size_t data_size = image_info.computeMinByteSize();

  // We need to cache the result of color conversion on the cpu if the image
  // will be color converted during the decode.
  auto decode_color_space = ColorSpaceForImageDecode(draw_image, mode);
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
  return base::WrapRefCounted(
      new ImageData(draw_image.paint_image().stable_id(), mode, data_size,
                    draw_image.target_color_space(),
                    CalculateDesiredFilterQuality(draw_image),
                    upload_scale_mip_level, needs_mips, is_bitmap_backed));
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
    if (image_data->mode == DecodedDataMode::kGpu)
      images_pending_deletion_.push_back(image_data->upload.image());
    if (image_data->mode == DecodedDataMode::kTransferCache)
      ids_pending_deletion_.push_back(*image_data->upload.transfer_cache_id());
  }
  image_data->upload.Reset();
}

void GpuImageDecodeCache::UnlockImage(ImageData* image_data) {
  DCHECK(image_data->HasUploadedData());
  if (image_data->mode == DecodedDataMode::kGpu) {
    images_pending_unlock_.push_back(image_data->upload.image().get());
  } else {
    DCHECK(image_data->mode == DecodedDataMode::kTransferCache);
    ids_pending_unlock_.push_back(*image_data->upload.transfer_cache_id());
  }
  image_data->upload.OnUnlock();

  // If we were holding onto an unmipped image for defering deletion, do it now
  // it is guarenteed to have no-refs.
  auto unmipped_image = image_data->upload.take_unmipped_image();
  if (unmipped_image)
    images_pending_deletion_.push_back(std::move(unmipped_image));
}

// We always run pending operations in the following order:
//   Lock > Unlock > Delete
// This ensures that:
//   a) We never fully unlock an image that's pending lock (lock before unlock)
//   b) We never delete an image that has pending locks/unlocks.
// As this can be run at-raster, to unlock/delete an image that was just used,
// we need to call GlIdFromSkImage, which flushes pending IO on the image,
// rather than just using a cached GL ID.
void GpuImageDecodeCache::RunPendingContextThreadOperations() {
  CheckContextLockAcquiredIfNecessary();
  lock_.AssertAcquired();

  for (auto* image : images_pending_complete_lock_) {
    context_->ContextSupport()->CompleteLockDiscardableTexureOnContextThread(
        GlIdFromSkImage(image));
  }
  images_pending_complete_lock_.clear();

  for (auto* image : images_pending_unlock_) {
    context_->ContextGL()->UnlockDiscardableTextureCHROMIUM(
        GlIdFromSkImage(image));
  }
  images_pending_unlock_.clear();

  for (auto id : ids_pending_unlock_) {
    context_->ContextSupport()->UnlockTransferCacheEntries({std::make_pair(
        static_cast<uint32_t>(TransferCacheEntryType::kImage), id)});
  }
  ids_pending_unlock_.clear();

  for (auto& image : images_pending_deletion_) {
    uint32_t texture_id = GlIdFromSkImage(image.get());
    if (context_->ContextGL()->LockDiscardableTextureCHROMIUM(texture_id)) {
      context_->ContextGL()->DeleteTextures(1, &texture_id);
    }
  }
  images_pending_deletion_.clear();

  for (auto id : ids_pending_deletion_) {
    if (context_->ContextSupport()->ThreadsafeLockTransferCacheEntry(
            static_cast<uint32_t>(TransferCacheEntryType::kImage), id)) {
      context_->ContextSupport()->DeleteTransferCacheEntry(
          static_cast<uint32_t>(TransferCacheEntryType::kImage), id);
    }
  }
  ids_pending_deletion_.clear();
}

SkImageInfo GpuImageDecodeCache::CreateImageInfoForDrawImage(
    const DrawImage& draw_image,
    int upload_scale_mip_level) const {
  gfx::Size mip_size =
      CalculateSizeForMipLevel(draw_image, upload_scale_mip_level);
  return SkImageInfo::Make(mip_size.width(), mip_size.height(), color_type_,
                           kPremul_SkAlphaType);
}

bool GpuImageDecodeCache::TryLockImage(HaveContextLock have_context_lock,
                                       const DrawImage& draw_image,
                                       ImageData* data) {
  DCHECK(data->HasUploadedData());

  if (data->upload.is_locked())
    return true;

  if (data->mode == DecodedDataMode::kTransferCache) {
    DCHECK(use_transfer_cache_);
    DCHECK(data->upload.transfer_cache_id());
    if (context_->ContextSupport()->ThreadsafeLockTransferCacheEntry(
            static_cast<uint32_t>(TransferCacheEntryType::kImage),
            *data->upload.transfer_cache_id())) {
      data->upload.OnLock();
      return true;
    }
  } else if (have_context_lock == HaveContextLock::kYes &&
             context_->ContextGL()->LockDiscardableTextureCHROMIUM(
                 data->upload.gl_id())) {
    DCHECK(!use_transfer_cache_);
    DCHECK(data->mode == DecodedDataMode::kGpu);
    // If |have_context_lock|, we can immediately lock the image and send
    // the lock command to the GPU process.
    data->upload.OnLock();
    return true;
  } else if (context_->ContextSupport()
                 ->ThreadSafeShallowLockDiscardableTexture(
                     data->upload.gl_id())) {
    DCHECK(!use_transfer_cache_);
    DCHECK(data->mode == DecodedDataMode::kGpu);
    // If !|have_context_lock|, we use ThreadsafeShallowLockDiscardableTexture.
    // This takes a reference to the image, ensuring that it can't be deleted
    // by the service, but delays sending a lock command over the command
    // buffer. This command must be sent before the image is used, but is now
    // guaranteed to succeed. We will send this command via
    // CompleteLockDiscardableTextureOnContextThread in UploadImageIfNecessary,
    // which is guaranteed to run before the texture is used.
    data->upload.OnLock();
    images_pending_complete_lock_.push_back(data->upload.image().get());
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
    const InUseCacheKey& key) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetImageDataForDrawImage");
  lock_.AssertAcquired();
  DCHECK(UseCacheForDrawImage(draw_image));

  auto found_in_use = in_use_cache_.find(key);
  if (found_in_use != in_use_cache_.end())
    return found_in_use->second.image_data.get();

  auto found_persistent = persistent_cache_.Get(draw_image.frame_key());
  if (found_persistent != persistent_cache_.end()) {
    ImageData* image_data = found_persistent->second.get();
    if (IsCompatible(image_data, draw_image)) {
      return image_data;
    } else {
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
  bool is_scaled = image_data->upload_scale_mip_level != 0;
  bool scale_is_compatible = CalculateUploadScaleMipLevel(draw_image) >=
                             image_data->upload_scale_mip_level;
  bool quality_is_compatible =
      CalculateDesiredFilterQuality(draw_image) <= image_data->quality;
  bool color_is_compatible =
      image_data->target_color_space == draw_image.target_color_space();
  if (!color_is_compatible)
    return false;
  if (is_scaled && (!scale_is_compatible || !quality_is_compatible))
    return false;
  return true;
}

size_t GpuImageDecodeCache::GetDrawImageSizeForTesting(const DrawImage& image) {
  base::AutoLock lock(lock_);
  scoped_refptr<ImageData> data = CreateImageData(image);
  return data->size;
}

void GpuImageDecodeCache::SetImageDecodingFailedForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.frame_key());
  DCHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  image_data->decode.decode_failure = true;
}

bool GpuImageDecodeCache::DiscardableIsLockedForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.frame_key());
  DCHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  return image_data->decode.is_locked();
}

bool GpuImageDecodeCache::IsInInUseCacheForTesting(
    const DrawImage& image) const {
  auto found = in_use_cache_.find(InUseCacheKey::FromDrawImage(image));
  return found != in_use_cache_.end();
}

bool GpuImageDecodeCache::IsInPersistentCacheForTesting(
    const DrawImage& image) const {
  auto found = persistent_cache_.Peek(image.frame_key());
  return found != persistent_cache_.end();
}

sk_sp<SkImage> GpuImageDecodeCache::GetSWImageDecodeForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.frame_key());
  DCHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  return image_data->decode.ImageForTesting();
}

void GpuImageDecodeCache::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  base::AutoLock lock(lock_);
  switch (level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      base::AutoReset<bool> reset(&aggressively_freeing_resources_, true);
      EnsureCapacity(0);
      break;
  }
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
    const DrawImage& image,
    DecodedDataMode mode) const {
  if (!SupportsColorSpaceConversion())
    return nullptr;

  if (mode == DecodedDataMode::kCpu)
    return image.target_color_space().ToSkColorSpace();

  // For kGpu or kTransferCache images color conversion is handled during
  // upload, so keep the original colorspace here.
  return sk_ref_sp(image.paint_image().color_space());
}

void GpuImageDecodeCache::CheckContextLockAcquiredIfNecessary() {
  if (!context_->GetLock())
    return;
  context_->GetLock()->AssertAcquired();
}

void GpuImageDecodeCache::UpdateMipsIfNeeded(const DrawImage& draw_image,
                                             ImageData* image_data) {
  CheckContextLockAcquiredIfNecessary();

  // If we already have mips, nothing to do.
  if (image_data->needs_mips)
    return;

  bool needs_mips =
      ShouldGenerateMips(draw_image, image_data->upload_scale_mip_level);
  if (!needs_mips)
    return;

  image_data->needs_mips = true;

  // If we have no uploaded image, nothing to do other than update needs_mips.
  // Mips will be generated during later upload.
  if (!image_data->HasUploadedData() ||
      image_data->mode != DecodedDataMode::kGpu)
    return;

  // Need to generate mips. Take a reference on the image we're about to delete,
  // delaying deletion.
  sk_sp<SkImage> previous_image = image_data->upload.image();

  // Generate a new image from the previous, adding mips.
  sk_sp<SkImage> image_with_mips = previous_image->makeTextureImage(
      context_->GrContext(), nullptr, GrMipMapped::kYes);

  // Handle lost context.
  if (!image_with_mips)
    return;

  // No need to do anything if mipping this image results in the same texture.
  // Deleting it below will result in lifetime issues.
  if (GlIdFromSkImage(image_with_mips.get()) == image_data->upload.gl_id())
    return;

  // Skia owns our new image, take ownership.
  sk_sp<SkImage> image_with_mips_owned = TakeOwnershipOfSkImageBacking(
      context_->GrContext(), std::move(image_with_mips));

  // Handle lost context
  if (!image_with_mips_owned)
    return;

  // The previous image might be in the in-use cache, potentially held
  // externally. We must defer deleting it until the entry is unlocked.
  image_data->upload.set_unmipped_image(image_data->upload.image());

  // Set the new image on the cache.
  image_data->upload.Reset();
  image_data->upload.SetImage(std::move(image_with_mips_owned));
  context_->ContextGL()->InitializeDiscardableTextureCHROMIUM(
      image_data->upload.gl_id());
}

}  // namespace cc
