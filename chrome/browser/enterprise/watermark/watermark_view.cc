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

const SkFont& WatermarkFont() {
  static SkFont font = []() {
    sk_sp<SkFontMgr> font_mgr = skia::DefaultFontMgr();
    sk_sp<SkTypeface> typeface = font_mgr->legacyMakeTypeface(
        "Arial",
        SkFontStyle(SkFontStyle::kExtraBold_Weight, SkFontStyle::kNormal_Width,
                    SkFontStyle::kUpright_Slant));
    return SkFont(typeface, kTextSize, 1.0f, 0.0f);
  }();
  return font;
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
  text_lines_ = base::SplitString(text, "\n", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY);

  // Block size calculation is only required when text is changed.
  block_width_ = 0;
  for (const auto& line : text_lines_) {
    block_width_ =
        std::max(SkScalar(block_width_),
                 WatermarkFont().measureText(line.c_str(), line.size(),
                                             SkTextEncoding::kUTF8));
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

  int horizontal_offset = block_width_ + kWatermarkBlockSpacing;
  int stagger_offset = horizontal_offset / 2;
  int vertical_offset = block_height_ + kWatermarkBlockSpacing;

  for (int x = 0; x <= bounds.width(); x += horizontal_offset) {
    bool apply_stagger = false;
    for (int y = kTextSize; y < bounds.height(); y += vertical_offset) {
      // Every other row, stagger the text horizontally to give a "brick tiling"
      // effect.
      int stagger = apply_stagger ? stagger_offset : 0;
      apply_stagger = !apply_stagger;

      DrawTextBlock(sk_canvas, x - stagger, y);
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

void WatermarkView::DrawTextBlock(cc::PaintCanvas* canvas, int x, int y) {
  static SkColor kOutlineColor = SkColorSetARGB(0x25, 0xff, 0xff, 0xff);
  static float kOutlineThickness = 1.0f;
  static SkColor kFillColor = SkColorSetARGB(0x20, 0x00, 0x00, 0x00);

  for (const std::string& line : text_lines_) {
    if (line.empty()) {
      // This condition should not happen since the SplitString() call to create
      // the string vector trims whitespace.
      NOTREACHED();
      continue;
    }

    sk_sp<SkTextBlob> blob =
        SkTextBlob::MakeFromString(line.c_str(), WatermarkFont());

    // Draw a stroke for the light-colored outline of the text.
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setColor(kOutlineColor);
    flags.setStrokeWidth(kOutlineThickness);
    canvas->drawTextBlob(blob, x, y, flags);

    // Draw the dark-colored fill of the text.
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(kFillColor);
    canvas->drawTextBlob(blob, x, y, flags);

    // Re-calculate `y` every loop so the next line is below the previous one.
    y += kTextSize;
  }
}

BEGIN_METADATA(WatermarkView)
END_METADATA

}  // namespace enterprise_watermark
