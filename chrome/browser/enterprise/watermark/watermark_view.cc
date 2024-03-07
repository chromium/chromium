// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_view.h"

#include <math.h>
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
constexpr double kRotationAngle = 45;

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
  text_blocks_.clear();
  std::vector<std::string> text_lines = base::SplitString(
      text, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Block size calculation is only required when text is changed.
  block_width_ = 0;
  for (const auto& line : text_lines) {
    block_width_ =
        std::max(SkScalar(block_width_),
                 WatermarkFont().measureText(line.c_str(), line.size(),
                                             SkTextEncoding::kUTF8));
    text_blocks_.push_back(
        SkTextBlob::MakeFromString(line.c_str(), WatermarkFont()));
  }
  block_height_ = kTextSize * text_lines.size();

  // Invalidate the state of the view.
  SchedulePaint();
}

void WatermarkView::OnPaint(gfx::Canvas* canvas) {
  // Trying to render an empty string in Skia will fail. A string is required
  // to create the command buffer for the renderer.
  if (text_blocks_.empty()) {
    return;
  }

  // Get ptr to Skia canvas.
  cc::PaintCanvas* sk_canvas = canvas->sk_canvas();
  sk_canvas->rotate(360 - kRotationAngle);

  // Get contents are in order to center the text inside it.
  gfx::Rect bounds = GetContentsBounds();

  int upper_x = max_x(kRotationAngle, bounds);
  int upper_y = max_y(kRotationAngle, bounds);
  for (int x = min_x(kRotationAngle, bounds); x <= upper_x;
       x += block_width_offset()) {
    bool apply_stagger = false;
    for (int y = min_y(kRotationAngle, bounds); y < upper_y;
         y += block_height_offset()) {
      // Every other row, stagger the text horizontally to give a
      // "brick tiling" effect.
      int stagger = apply_stagger ? block_width_offset() / 2 : 0;
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

  for (const sk_sp<SkTextBlob>& text_block : text_blocks_) {
    // Draw a stroke for the light-colored outline of the text.
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setColor(kOutlineColor);
    flags.setStrokeWidth(kOutlineThickness);
    canvas->drawTextBlob(text_block, x, y, flags);

    // Draw the dark-colored fill of the text.
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(kFillColor);
    canvas->drawTextBlob(text_block, x, y, flags);

    // Re-calculate `y` every loop so the next line is below the previous one.
    y += kTextSize;
  }
}

int WatermarkView::block_width_offset() const {
  return block_width_ + kWatermarkBlockSpacing;
}

int WatermarkView::block_height_offset() const {
  return block_height_ + kWatermarkBlockSpacing;
}

int WatermarkView::min_x(double angle, const gfx::Rect& bounds) const {
  // Due to the rotation of the watermark, X needs to start in the negatives so
  // that the rotated canvas is still large enough to cover `bounds`. This means
  // our initial X needs to be proportional to this triangle side:
  //             |
  //   +---------+
  //   |
  //   |     ╱angle
  //   |    ╱┌────────────────────
  //   V   ╱ │
  //      ╱  │
  //   X ╱   │
  //    ╱    │
  //   ╱     │  `bounds`
  //  ╱90    │
  //  ╲deg.  │
  //   ╲     │
  //    ╲    │
  //     ╲   │
  //      ╲  │
  //       ╲ │
  //        ╲│
  //
  // -X also needs to be a factor of `block_width_offset()` so that there is no
  // sliding of the watermark blocks when `bounds` resize and there's always a
  // text block drawn at X=0.
  int min = cos(90 - angle) * bounds.height();
  return -((min / block_width_offset()) + 1) * block_width_offset();
}

int WatermarkView::max_x(double angle, const gfx::Rect& bounds) const {
  // Due to the rotation of the watermark, X needs to end further then the
  // `bounds` width. This means our final X needs to be proportional to this
  // triangle side:
  //           |
  //           |
  //           |     ╱╲
  //           |    ╱90╲
  //           V   ╱deg.╲
  //              ╱      ╲
  //           X ╱        ╲
  //            ╱          ╲
  //           ╱            ╲
  //          ╱              ╲
  //         ╱angle           ╲
  //        ┌──────────────────┐
  //        │  `bounds`        │
  //
  // An extra `block_width_offset()` length is added so that the last column for
  // staggered rows doesn't appear on resizes.
  return cos(angle) * bounds.width() + block_width_offset();
}

int WatermarkView::min_y(double angle, const gfx::Rect& bounds) const {
  // Instead of starting at Y=0, starting at `kTextSize` lets the first line of
  // text be in frame as text is drawn with (0,0) as the bottom-left corner.
  return kTextSize;
}

int WatermarkView::max_y(double angle, const gfx::Rect& bounds) const {
  // Due to the rotation of the watermark, Y needs to end further then the
  // `bounds` height. This means our final Y needs to be proportional to these
  // two triangle sides:  +-----------+
  //                      |           |
  //                      |           |
  //                 ╱╲   V           |
  //                ╱90╲              |
  //               ╱deg.╲ Y1          |
  //              ╱      ╲            |
  //             ╱        ╲           |
  //            ╱          ╲          |
  //           ╱            ╲         |
  //          ╱              ╲        |
  //         ╱angle           ╲       |
  //        ┌──────────────────┐      |
  //        │  `bounds`        │╲     |
  //                           │ ╲    |
  //                           │  ╲   V
  //                           │   ╲
  //                           │    ╲ Y2
  //                           │     ╲
  //                           │      ╲
  //                           │    90 ╲
  //                           │   deg.╱
  //                           │      ╱
  //                           │     ╱
  //                           │    ╱
  //                           │   ╱
  //                           │  ╱
  //                           │ ╱
  //                           │╱
  //
  return sin(angle) * bounds.width() + cos(angle) * bounds.height();
}

BEGIN_METADATA(WatermarkView)
END_METADATA

}  // namespace enterprise_watermark
