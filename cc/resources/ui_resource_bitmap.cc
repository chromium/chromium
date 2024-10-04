// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/ui_resource_bitmap.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrRecordingContext.h"

namespace cc {
namespace {

UIResourceBitmap::UIResourceFormat SkColorTypeToUIResourceFormat(
    SkColorType sk_type) {
  UIResourceBitmap::UIResourceFormat format = UIResourceBitmap::RGBA8;
  switch (sk_type) {
    case kN32_SkColorType:
      format = UIResourceBitmap::RGBA8;
      break;
    case kAlpha_8_SkColorType:
      format = UIResourceBitmap::ALPHA_8;
      break;
    default:
      NOTREACHED() << "Invalid SkColorType for UIResourceBitmap: " << sk_type;
  }
  return format;
}

}  // namespace

void UIResourceBitmap::Create(sk_sp<SkPixelRef> pixel_ref,
                              const SkImageInfo& info,
                              UIResourceFormat format) {
  DCHECK(info.width());
  DCHECK(info.height());
  DCHECK(pixel_ref);
  DCHECK(pixel_ref->isImmutable());
  format_ = format;
  info_ = info;
  pixel_ref_ = std::move(pixel_ref);
}

void UIResourceBitmap::DrawToCanvas(SkCanvas* canvas, SkPaint* paint) {
  DCHECK_NE(info_.colorType(), kUnknown_SkColorType);

  SkBitmap bitmap;
  bitmap.setInfo(info_, pixel_ref_.get()->rowBytes());
  bitmap.setPixelRef(pixel_ref_, 0, 0);
  canvas->drawImage(bitmap.asImage(), 0, 0, SkSamplingOptions(), paint);
  if (GrDirectContext* direct_context =
          GrAsDirectContext(canvas->recordingContext())) {
    direct_context->flushAndSubmit();
  }
}

base::span<const uint8_t> UIResourceBitmap::GetPixels() const {
  if (!pixel_ref_) {
    return {};
  }
  // TODO(crbug.com/40285824): Check if this is guaranteed safe. The pixel
  // memory must be at least row_bytes * height but it's not well defined if
  // memory past the end of the last row is allocated when row_bytes > width *
  // bytes_per_pixel. UIResourceBitmap has an implicit assumption that row_bytes
  // == width * bytes_per_pixel but if that assumption is violated this span
  // could be too large.
  return UNSAFE_TODO(base::span(
      static_cast<const uint8_t*>(pixel_ref_->pixels()), SizeInBytes()));
}

size_t UIResourceBitmap::SizeInBytes() const {
  if (!pixel_ref_)
    return 0u;
  base::CheckedNumeric<size_t> size_in_bytes = pixel_ref_->rowBytes();
  size_in_bytes *= info_.height();
  return size_in_bytes.ValueOrDie();
}

UIResourceBitmap::UIResourceBitmap(const SkBitmap& skbitmap) {
  DCHECK(skbitmap.isImmutable());

  const SkBitmap* target = &skbitmap;
#if BUILDFLAG(IS_ANDROID)
  SkBitmap copy;
  if (features::IsDrDcEnabled()) {
    // TODO(vikassoni): Forcing everything to N32 while android backing cannot
    // support some other formats. Note that DrDc is disabled on some gl
    // renderers and hence gpus via gpu driver bug workaround. That workaround
    // is not applied here and so on those disable gpus, everything will still
    // be forced to N32 even though drdc is disabled. This should be fine for
    // now and would be fixed later. crbug.com/1354201.
    if (skbitmap.colorType() != kN32_SkColorType) {
      SkImageInfo new_info = skbitmap.info().makeColorType(kN32_SkColorType);
      copy.allocPixels(new_info, new_info.minRowBytes());
      SkCanvas copy_canvas(copy);
      copy_canvas.drawImage(skbitmap.asImage(), 0, 0, SkSamplingOptions(),
                            nullptr);
      copy.setImmutable();
      target = &copy;
    }
    DCHECK_EQ(target->width(), target->rowBytesAsPixels());
    DCHECK(target->isImmutable());
  }
#endif
  sk_sp<SkPixelRef> pixel_ref = sk_ref_sp(target->pixelRef());
  Create(std::move(pixel_ref), target->info(),
         SkColorTypeToUIResourceFormat(target->colorType()));
}

UIResourceBitmap::UIResourceBitmap(const gfx::Size& size, bool is_opaque) {
  SkAlphaType alphaType = is_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
  SkImageInfo info =
      SkImageInfo::MakeN32(size.width(), size.height(), alphaType);
  sk_sp<SkPixelRef> pixel_ref(
      SkMallocPixelRef::MakeAllocate(info, info.minRowBytes()));
  pixel_ref->setImmutable();
  Create(std::move(pixel_ref), info, UIResourceBitmap::RGBA8);
}

UIResourceBitmap::UIResourceBitmap(sk_sp<SkPixelRef> pixel_ref,
                                   const gfx::Size& size) {
  // TODO(khushalsagar): It doesn't make sense to SkPixelRef to pass around
  // encoded data.
  SkImageInfo info = SkImageInfo::Make(
      size.width(), size.height(), kUnknown_SkColorType, kOpaque_SkAlphaType);
  Create(std::move(pixel_ref), info, UIResourceBitmap::ETC1);
}

UIResourceBitmap::UIResourceBitmap(const UIResourceBitmap& other) = default;

UIResourceBitmap::~UIResourceBitmap() = default;

SkBitmap UIResourceBitmap::GetBitmapForTesting() const {
  SkBitmap bitmap;
  bitmap.setInfo(info_);
  bitmap.setPixelRef(pixel_ref_, 0, 0);
  return bitmap;
}

}  // namespace cc
