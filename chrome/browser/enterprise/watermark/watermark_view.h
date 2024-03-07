// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_VIEW_H_
#define CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_VIEW_H_

#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace cc {
class PaintCanvas;
}

namespace gfx {
class Canvas;
}

namespace enterprise_watermark {

// WatermarkView represents a view that can superimpose a watermark on top of
// other views. The view should be appropriately sized using its parent's layout
// manager.
class WatermarkView : public views::View {
  METADATA_HEADER(WatermarkView, views::View)

 public:
  WatermarkView();
  explicit WatermarkView(std::string text);
  ~WatermarkView() override;

  // Set this to a translucent value for testing. Useful for visualizing the
  // view's bounds when performing transformations.
  void SetBackgroundColor(SkColor color);

  void SetString(const std::string& text);

  // views::View
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // Helper function to draw a single block of text with the data in
  // `text_blocks_` at the provided coordinates.
  void DrawTextBlock(cc::PaintCanvas* canvas, int x, int y);

  // The width/height of individual blocks of text, including spacing.
  int block_width_offset() const;
  int block_height_offset() const;

  // Calculate X/Y bounds for the rotates canvas to cover the entire `bounds`
  // the waternarj is overlapping.
  int min_x(double angle, const gfx::Rect& bounds) const;
  int max_x(double angle, const gfx::Rect& bounds) const;
  int min_y(double angle, const gfx::Rect& bounds) const;
  int max_y(double angle, const gfx::Rect& bounds) const;

  std::vector<sk_sp<SkTextBlob>> text_blocks_;
  SkColor background_color_;
  int block_width_;
  int block_height_;
};

}  // namespace enterprise_watermark

#endif
