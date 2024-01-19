// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_view.h"

#include "cc/paint/paint_canvas.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"

namespace enterprise_watermark {

WatermarkView::WatermarkView() : WatermarkView(std::string("")) {}

WatermarkView::WatermarkView(std::string text) : text_(std::move(text)) {}

WatermarkView::~WatermarkView() = default;

void WatermarkView::OnPaint(gfx::Canvas* canvas) {
  // Get ptr to Skia canvas
  cc::PaintCanvas* sk_canvas = canvas->sk_canvas();

  // Set up font properties
  sk_sp<SkFontMgr> font_mgr = skia::DefaultFontMgr();
  sk_sp<SkTypeface> typeface = font_mgr->legacyMakeTypeface(
      "Arial",
      SkFontStyle(SkFontStyle::kExtraBold_Weight, SkFontStyle::kNormal_Width,
                  SkFontStyle::kUpright_Slant));
  SkFont font(typeface, 30.0f, 1.0f, 0.0f);

  // Get contents are in order to center the text inside it
  gfx::Rect bounds = GetContentsBounds();

  // Text must be manually centered
  SkScalar text_width =
      font.measureText(text_.c_str(), text_.size(), SkTextEncoding::kUTF8);
  int x = (bounds.width() - text_width) / 2;
  int y = bounds.height() / 2;

  // Create text blob
  sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromString(text_.c_str(), font);

  // Draw stroke
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setColor(SkColorSetARGB(0x25, 0xff, 0xff, 0xff));
  flags.setStrokeWidth(1.0f);
  sk_canvas->drawTextBlob(blob, x, y, flags);

  // Draw fill
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(SkColorSetARGB(0x20, 0x0, 0x0, 0x0));
  sk_canvas->drawTextBlob(blob, x, y, flags);
}

BEGIN_METADATA(WatermarkView, views::View)
END_METADATA

}  // namespace enterprise_watermark
