// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_view.h"

#include <math.h>

#include <algorithm>
#include <string>

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

namespace enterprise_watermark {

WatermarkView::WatermarkView() : WatermarkView(std::string("")) {}

WatermarkView::WatermarkView(std::string text)
    : background_color_(SkColorSetARGB(0, 0, 0, 0)) {
  SetCanProcessEventsWithinSubtree(false);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetString(text);
  GetViewAccessibility().SetIsInvisible(true);
}

WatermarkView::~WatermarkView() = default;

void WatermarkView::SetString(const std::string& text) {
  DCHECK(base::IsStringUTF8(text));

  watermark_block_ =
      DrawWatermarkToPaintRecord(text, GetFillColor(), GetOutlineColor());

  // Invalidate the state of the view.
  SchedulePaint();
}

void WatermarkView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect contents_bounds = GetContentsBounds();
  DrawWatermark(
      canvas->sk_canvas(), &watermark_block_.record, watermark_block_.width,
      watermark_block_.height,
      SkSize::Make(contents_bounds.width(), contents_bounds.height()));
}

void WatermarkView::SetBackgroundColor(SkColor background_color) {
  background_color_ = background_color;
  SchedulePaint();
}

BEGIN_METADATA(WatermarkView)
END_METADATA

}  // namespace enterprise_watermark
