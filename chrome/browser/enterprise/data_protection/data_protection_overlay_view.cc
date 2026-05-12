// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_overlay_view.h"

#include <math.h>

#include <algorithm>
#include <string>

#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_recorder.h"
#include "chrome/browser/enterprise/watermark/settings.h"
#include "components/enterprise/watermarking/watermark.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/render_text.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace enterprise_data_protection {

DataProtectionOverlayView::DataProtectionOverlayView()
    : background_color_(SkColorSetARGB(0, 0, 0, 0)) {
  SetCanProcessEventsWithinSubtree(false);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  UpdateWatermarkBlock(/*watermark_text=*/std::string(""),
                       enterprise_watermark::GetDefaultFillColor(),
                       enterprise_watermark::GetDefaultOutlineColor(),
                       enterprise_watermark::GetDefaultFontSize());
  GetViewAccessibility().SetIsInvisible(true);
}

DataProtectionOverlayView::~DataProtectionOverlayView() = default;

void DataProtectionOverlayView::InvalidateView() {
  SchedulePaint();
}

void DataProtectionOverlayView::MaybeUpdateWatermarkBlock(
    const std::string& watermark_text,
    SkColor fill_color,
    SkColor outline_color,
    int font_size) {
  // No need to invalidate when both old and new strings are empty, even if the
  // style values change.
  if (watermark_text_.empty() && watermark_text.empty()) {
    return;
  }

  // If at least one value changes, invalidate the view.
  bool should_update =
      (watermark_text_ != watermark_text) || (fill_color_ != fill_color) ||
      (outline_color_ != outline_color) || (font_size_ != font_size);

  if (should_update) {
    watermark_text_ = watermark_text;
    fill_color_ = fill_color;
    outline_color_ = outline_color;
    font_size_ = font_size;

    UpdateWatermarkBlock(watermark_text, fill_color, outline_color, font_size);
  }
}

void DataProtectionOverlayView::SetWatermarkText(const std::string& text,
                                                 SkColor fill_color,
                                                 SkColor outline_color,
                                                 int font_size) {
  DCHECK(base::IsStringUTF8(text));
  CHECK_GE(font_size, 1);

  MaybeUpdateWatermarkBlock(text, fill_color, outline_color, font_size);
}

void DataProtectionOverlayView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect contents_bounds = GetContentsBounds();
  enterprise_watermark::DrawWatermark(
      canvas->sk_canvas(), &watermark_block_.record, watermark_block_.width,
      watermark_block_.height,
      SkSize::Make(contents_bounds.width(), contents_bounds.height()));
}

void DataProtectionOverlayView::UpdateWatermarkBlock(const std::string& text,
                                                     SkColor fill_color,
                                                     SkColor outline_color,
                                                     int font_size) {
  watermark_block_ = enterprise_watermark::DrawWatermarkToPaintRecord(
      text, fill_color, outline_color, font_size);
  InvalidateView();
}

void DataProtectionOverlayView::SetBackgroundColor(SkColor background_color) {
  background_color_ = background_color;
  InvalidateView();
}

BEGIN_METADATA(DataProtectionOverlayView)
END_METADATA

}  // namespace enterprise_data_protection
