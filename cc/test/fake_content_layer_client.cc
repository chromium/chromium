// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer_client.h"

#include <algorithm>
#include <cstddef>

#include "cc/paint/paint_op_buffer.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {

FakeContentLayerClient::ImageData::ImageData(PaintImage img,
                                             const gfx::Point& point,
                                             const SkSamplingOptions& sampling,
                                             const PaintFlags& flags)
    : image(std::move(img)), point(point), sampling(sampling), flags(flags) {}

FakeContentLayerClient::ImageData::ImageData(PaintImage img,
                                             const gfx::Transform& transform,
                                             const SkSamplingOptions& sampling,
                                             const PaintFlags& flags)
    : image(std::move(img)),
      transform(transform),
      sampling(sampling),
      flags(flags) {}

FakeContentLayerClient::ImageData::ImageData(const ImageData& other) = default;

FakeContentLayerClient::ImageData::~ImageData() = default;

FakeContentLayerClient::SkottieData::SkottieData(
    scoped_refptr<SkottieWrapper> skottie,
    const gfx::Rect& dst,
    float t,
    SkottieFrameDataMap images,
    SkottieColorMap color_map,
    SkottieTextPropertyValueMap text_map)
    : skottie(std::move(skottie)),
      dst(dst),
      t(t),
      images(std::move(images)),
      color_map(std::move(color_map)),
      text_map(std::move(text_map)) {}

FakeContentLayerClient::SkottieData::SkottieData(const SkottieData& other) =
    default;

FakeContentLayerClient::SkottieData&
FakeContentLayerClient::SkottieData::operator=(const SkottieData& other) =
    default;

FakeContentLayerClient::SkottieData::~SkottieData() = default;

FakeContentLayerClient::FakeContentLayerClient() = default;

FakeContentLayerClient::~FakeContentLayerClient() = default;

scoped_refptr<DisplayItemList>
FakeContentLayerClient::PaintContentsToDisplayList() {
  if (display_list_) {
    return display_list_;
  }

  DCHECK(bounds_set_);
  gfx::Rect paint_bounds(bounds_);
  auto display_list = base::MakeRefCounted<DisplayItemList>();

  for (RectPaintVector::const_iterator it = draw_rects_.begin();
       it != draw_rects_.end(); ++it) {
    const gfx::RectF& draw_rect = it->first;
    const PaintFlags& flags = it->second;

    display_list->StartPaint();
    display_list->push<DrawRectOp>(gfx::RectFToSkRect(draw_rect), flags);
    display_list->EndPaintOfUnpaired(ToEnclosingRect(draw_rect));
  }

  for (ImageVector::const_iterator it = draw_images_.begin();
       it != draw_images_.end(); ++it) {
    if (!it->transform.IsIdentity()) {
      display_list->StartPaint();
      display_list->push<SaveOp>();
      display_list->push<ConcatOp>(gfx::TransformToSkM44(it->transform));
      display_list->EndPaintOfPairedBegin();
    }

    display_list->StartPaint();
    display_list->push<SaveOp>();
    display_list->push<ClipRectOp>(gfx::RectToSkRect(paint_bounds),
                                   SkClipOp::kIntersect, false);
    display_list->push<DrawImageOp>(
        it->image, static_cast<float>(it->point.x()),
        static_cast<float>(it->point.y()),
        PaintFlags::FilterQualityToSkSamplingOptions(
            it->flags.getFilterQuality()),
        &it->flags);
    display_list->push<RestoreOp>();
    display_list->EndPaintOfUnpaired(paint_bounds);

    if (!it->transform.IsIdentity()) {
      display_list->StartPaint();
      display_list->push<RestoreOp>();
      display_list->EndPaintOfPairedEnd();
    }
  }

  for (const SkottieData& skottie_data : skottie_data_) {
    display_list->StartPaint();
    display_list->push<DrawSkottieOp>(
        skottie_data.skottie, gfx::RectToSkRect(skottie_data.dst),
        skottie_data.t, skottie_data.images, skottie_data.color_map,
        skottie_data.text_map);
    display_list->EndPaintOfUnpaired(paint_bounds);
  }

  if (contains_slow_paths_) {
    // Add 6 slow paths, passing the reporting threshold.
    SkPath path;
    path.addCircle(2, 2, 5);
    path.addCircle(3, 4, 2);
    display_list->StartPaint();
    for (int i = 0; i < 6; ++i) {
      display_list->push<ClipPathOp>(path, SkClipOp::kIntersect, true);
    }
    display_list->EndPaintOfUnpaired(paint_bounds);
  }

  if (fill_with_nonsolid_color_) {
    PaintFlags flags;
    flags.setColor(SK_ColorRED);

    display_list->StartPaint();
    gfx::Rect draw_rect = paint_bounds;
    while (!draw_rect.IsEmpty()) {
      display_list->push<DrawIRectOp>(gfx::RectToSkIRect(draw_rect), flags);
      draw_rect.Inset(1);
    }
    display_list->EndPaintOfUnpaired(paint_bounds);
  }

  if (has_non_aa_paint_) {
    PaintFlags flags;
    flags.setAntiAlias(false);
    display_list->StartPaint();
    display_list->push<DrawRectOp>(SkRect::MakeWH(10, 10), flags);
    display_list->EndPaintOfUnpaired(paint_bounds);
  }

  if (has_draw_text_op_) {
    display_list->StartPaint();
    SkFont font = skia::DefaultFont();
    display_list->push<DrawTextBlobOp>(SkTextBlob::MakeFromString("any", font),
                                       0.0f, 0.0f, PaintFlags());
    display_list->EndPaintOfUnpaired(paint_bounds);
  }

  display_list->Finalize();
  return display_list;
}

bool FakeContentLayerClient::FillsBoundsCompletely() const { return false; }

}  // namespace cc
