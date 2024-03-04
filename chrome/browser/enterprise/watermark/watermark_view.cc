// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_view.h"

#include <algorithm>
#include "cc/paint/paint_canvas.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"

namespace enterprise_watermark {

namespace {
constexpr float kTextSize = 30.0f;
constexpr int kWatermarkBlockSpacing = 80;

SkFont CreateSkFont(const std::string& font_name) {
  sk_sp<SkFontMgr> font_mgr = skia::DefaultFontMgr();
  sk_sp<SkTypeface> typeface = font_mgr->legacyMakeTypeface(
      font_name.c_str(),
      SkFontStyle(SkFontStyle::kExtraBold_Weight, SkFontStyle::kNormal_Width,
                  SkFontStyle::kUpright_Slant));
  return SkFont(typeface, kTextSize, 1.0f, 0.0f);
}

}  // namespace

WatermarkView::WatermarkView() : WatermarkView(std::string("")) {}

WatermarkView::WatermarkView(std::string text)
    : background_color_(SkColorSetARGB(0, 0, 0, 0)) {
  SetCanProcessEventsWithinSubtree(false);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetString(text);
}

WatermarkView::~WatermarkView() = default;

void WatermarkView::SetString(const std::string& text) {
  SkFont font = CreateSkFont("Arial");

  text_lines_ = base::SplitString(text, "\n", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY);

  // Block size calculation is only required when text is changed.
  block_width_ = 0;
  for (const auto& line : text_lines_) {
    block_width_ = std::max(
        SkScalar(block_width_),
        font.measureText(line.c_str(), line.size(), SkTextEncoding::kUTF8));
  }
  block_height_ = kTextSize * text_lines_.size();

  // Invalidate the state of the view.
  SchedulePaint();
}

void WatermarkView::OnPaint(gfx::Canvas* canvas) {
  // Trying to render an empty string in Skia will fail. A string is required
  // to create the command buffer for the renderer.
  if (text_lines_.empty()) {
    return;
  }

  // Get ptr to Skia canvas.
  cc::PaintCanvas* sk_canvas = canvas->sk_canvas();

  // Get contents are in order to center the text inside it.
  gfx::Rect bounds = GetContentsBounds();
  int offset = (block_width_ + kWatermarkBlockSpacing) / 2;

  // Set up font properties.
  SkFont font = CreateSkFont("Arial");

  // Text must be manually positioned.
  for (size_t i = 0; i < text_lines_.size(); ++i) {
    const std::string& line = text_lines_[i];
    sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromString(line.c_str(), font);
    int base_x = 0;
    for (int y = kTextSize * (i + 1); y < bounds.height();
         y += block_height_ + kWatermarkBlockSpacing) {
      for (int x = base_x; x < bounds.width();
           x += block_width_ + kWatermarkBlockSpacing) {
        // The line.empty() condition should not happen, since the SplitString()
        // call to create the string vector trims whitespace. Added the check
        // below defensively.
        if (!line.empty()) {
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
      }
      base_x = offset - base_x;
    }
  }

  // Draw BG
  cc::PaintFlags bgflags;
  bgflags.setColor(background_color_);
  bgflags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawRect(GetLocalBounds(), bgflags);
}

void WatermarkView::SetBackgroundColor(SkColor background_color) {
  background_color_ = background_color;
  SchedulePaint();
}

BEGIN_METADATA(WatermarkView)
END_METADATA

}  // namespace enterprise_watermark
