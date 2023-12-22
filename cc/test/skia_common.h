// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SKIA_COMMON_H_
#define CC_TEST_SKIA_COMMON_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "cc/base/region.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/image_animation_count.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_generator.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace gfx {
class Rect;
class Size;
}

namespace cc {
class DisplayItemList;
class PaintWorkletInput;
class SkottieWrapper;

void DrawDisplayList(unsigned char* buffer,
                     const gfx::Rect& layer_rect,
                     scoped_refptr<const DisplayItemList> list);

bool AreDisplayListDrawingResultsSame(const gfx::Rect& layer_rect,
                                      const DisplayItemList* list_a,
                                      const DisplayItemList* list_b);

Region ImageRectsToRegion(const DiscardableImageMap::Rects& rects);

sk_sp<PaintImageGenerator> CreatePaintImageGenerator(const gfx::Size& size);

PaintImage CreatePaintWorkletPaintImage(scoped_refptr<PaintWorkletInput> input);

SkYUVAPixmapInfo GetYUVAPixmapInfo(const gfx::Size& image_size,
                                   YUVSubsampling yuv_format,
                                   SkYUVAPixmapInfo::DataType yuv_data_type,
                                   bool has_alpha = false);

PaintImage CreateDiscardablePaintImage(
    const gfx::Size& size,
    sk_sp<SkColorSpace> color_space = nullptr,
    bool allocate_encoded_memory = true,
    PaintImage::Id id = PaintImage::kInvalidId,
    SkColorType color_type = kN32_SkColorType,
    std::optional<YUVSubsampling> yuv_format = std::nullopt,
    SkYUVAPixmapInfo::DataType yuv_data_type =
        SkYUVAPixmapInfo::DataType::kUnorm8);

DrawImage CreateDiscardableDrawImage(const gfx::Size& size,
                                     sk_sp<SkColorSpace> color_space,
                                     SkRect rect,
                                     PaintFlags::FilterQuality filter_quality,
                                     const SkM44& matrix);

PaintImage CreateAnimatedImage(
    const gfx::Size& size,
    std::vector<FrameMetadata> frames,
    int repetition_count = kAnimationLoopInfinite,
    PaintImage::Id id = PaintImage::GetNextId());

PaintImage CreateBitmapImage(const gfx::Size& size,
                             SkColorType color_type = kN32_SkColorType);

scoped_refptr<SkottieWrapper> CreateSkottie(const gfx::Size& size,
                                            int duration_secs);
scoped_refptr<SkottieWrapper> CreateSkottieFromString(std::string_view json);
std::string LoadSkottieFileFromTestData(
    base::FilePath::StringPieceType animation_file_name);
scoped_refptr<SkottieWrapper> CreateSkottieFromTestDataDir(
    base::FilePath::StringPieceType animation_file_name);

PaintImage CreateNonDiscardablePaintImage(const gfx::Size& size);

}  // namespace cc

#endif  // CC_TEST_SKIA_COMMON_H_
