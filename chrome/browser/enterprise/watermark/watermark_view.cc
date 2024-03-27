// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_view.h"

#include <math.h>
#include <algorithm>
#include <string>

#include "base/no_destructor.h"
#include "cc/paint/paint_canvas.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/render_text.h"

namespace enterprise_watermark {

namespace {

// UX Requirements:
constexpr float kTextSize = 24.0f;
constexpr int kWatermarkBlockWidth = 350;
constexpr int kWatermarkBlockSpacing = 80;
constexpr double kRotationAngle = 45;
constexpr SkColor kFillColor = SkColorSetARGB(0x12, 0x00, 0x00, 0x00);
constexpr SkColor kOutlineColor = SkColorSetARGB(0x27, 0xff, 0xff, 0xff);

gfx::Font WatermarkFont() {
  return gfx::Font(
#if BUILDFLAG(IS_WIN)
      "Segoe UI",
#elif BUILDFLAG(IS_MAC)
      "SF Pro Text",
#elif BUILDFLAG(IS_LINUX)
      "Ubuntu",
#elif BUILDFLAG(IS_CHROMEOS)
      "Google Sans",
#else
      "sans-serif",
#endif
      kTextSize);
}

gfx::Font::Weight WatermarkFontWeight() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  return gfx::Font::Weight::SEMIBOLD;
#else
  return gfx::Font::Weight::MEDIUM;
#endif
}

const gfx::FontList& WatermarkFontList() {
  static base::NoDestructor<gfx::FontList> font_list(WatermarkFont());
  return *font_list;
}

std::unique_ptr<gfx::RenderText> CreateRenderText(const gfx::Rect& display_rect,
                                                  const std::u16string& text) {
  auto render_text = gfx::RenderText::CreateRenderText();
  render_text->set_clip_to_display_rect(false);
  render_text->SetFontList(WatermarkFontList());
  render_text->SetWeight(WatermarkFontWeight());
  render_text->SetDisplayOffset(gfx::Vector2d(0, 0));
  render_text->SetDisplayRect(display_rect);
  render_text->SetText(text);
  render_text->SetMultiline(true);
  render_text->SetWordWrapBehavior(gfx::WRAP_LONG_WORDS);
  return render_text;
}

std::unique_ptr<gfx::RenderText> CreateFillRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text) {
  auto render_text = CreateRenderText(display_rect, text);
  render_text->SetFillStyle(cc::PaintFlags::kFill_Style);
  render_text->SetColor(kFillColor);
  return render_text;
}

std::unique_ptr<gfx::RenderText> CreateOutlineRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text) {
  auto render_text = CreateRenderText(display_rect, text);
  render_text->SetFillStyle(cc::PaintFlags::kStroke_Style);
  render_text->SetColor(kOutlineColor);
  return render_text;
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
  DCHECK(base::IsStringUTF8(text));

  if (text.empty()) {
    text_fill_.reset();
    text_outline_.reset();
    block_height_ = 0;
  } else {
    std::u16string utf16_text = base::UTF8ToUTF16(text);

    // The coordinates here do not matter as the display rect will change for
    // each drawn block.
    gfx::Rect display_rect(0, 0, kWatermarkBlockWidth, 0);
    text_fill_ = CreateFillRenderText(display_rect, utf16_text);
    text_outline_ = CreateOutlineRenderText(display_rect, utf16_text);

    // `block_height_` is going to be the max required height for a single line
    // times the number of line.
    int w = kWatermarkBlockWidth;
    gfx::Canvas::SizeStringInt(utf16_text, WatermarkFontList(), &w,
                               &block_height_, kTextSize,
                               gfx::Canvas::NO_ELLIPSIS);
    block_height_ *= text_fill_->GetNumLines();
  }

  // Invalidate the state of the view.
  SchedulePaint();
}

void WatermarkView::OnPaint(gfx::Canvas* canvas) {
  // Trying to render an empty string in Skia will fail. A string is required
  // to create the command buffer for the renderer.
  if (!text_fill_) {
    DCHECK(!text_outline_);
    return;
  }

  canvas->sk_canvas()->rotate(360 - kRotationAngle);

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

      DrawTextBlock(canvas, x - stagger, y);
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

void WatermarkView::DrawTextBlock(gfx::Canvas* canvas, int x, int y) {
  gfx::Rect display_rect(x, y, kWatermarkBlockWidth, block_height_);

  text_fill_->SetDisplayRect(display_rect);
  text_fill_->Draw(canvas);

  text_outline_->SetDisplayRect(display_rect);
  text_outline_->Draw(canvas);
}

int WatermarkView::block_width_offset() const {
  return kWatermarkBlockWidth + kWatermarkBlockSpacing;
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
