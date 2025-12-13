// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_VIEW_H_
#define CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_VIEW_H_

#include "components/enterprise/watermarking/watermark.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
class RenderText;
}

namespace enterprise_watermark {

// WatermarkView represents a view that can superimpose a watermark on top of
// other views. The view should be appropriately sized using its parent's layout
// manager.
class WatermarkView : public views::View {
  METADATA_HEADER(WatermarkView, views::View)

 public:
  WatermarkView();
  ~WatermarkView() override;

  // Set this to a translucent value for testing. Useful for visualizing the
  // view's bounds when performing transformations.
  void SetBackgroundColor(SkColor color);

  // Convenience function to draw a simple, text-based watermark. `text` must be
  // UTF-8 encoded.
  // `font_size` must be a positive integer.
  void SetString(const std::string& text,
                 SkColor fill_color,
                 SkColor outline_color,
                 int font_size);

  // Alternative to SetString. Allows watermark to be set to any drawing
  // represented by a cc::PaintRecord instance.
  void SetWatermarkPaintRecord(cc::PaintRecord record);

  // views::View
  void OnPaint(gfx::Canvas* canvas) override;

  bool has_text_for_testing() const { return !watermark_block_.record.empty(); }

  // Wrapper of `SchedulePaint()`, virtual for testing.
  virtual void InvalidateView();

 private:
  // Returns true if the values have been updated.
  void MaybeUpdateWatermarkBlock(const std::string& watermark_text,
                                 SkColor fill_color,
                                 SkColor outline_color,
                                 int font_size);

  // Unconditionally update the `watermark_block_` attribute.
  void UpdateWatermarkBlock(const std::string& watermark_text,
                            SkColor fill_color,
                            SkColor outline_color,
                            int font_size);

  // Cached values of the watermark string and style to be rendered on the
  // watermark view. This is to avoid invalidating the view and triggering
  // redundant paints when the watermark is unchanged. The initial values are
  // arbitrary sensible defaults.
  std::string watermark_text_ = "";
  SkColor fill_color_ = SK_ColorTRANSPARENT;
  SkColor outline_color_ = SK_ColorTRANSPARENT;
  int font_size_ = 10;

  // Background color of the whole `WatermarkView`. This is normally
  // transparent, but can be an arbitrary color for testing with the
  // "watermark_app" target.
  SkColor background_color_;

  // Height required to draw all the lines in `text_fill_`/`text_outline_` in a
  // single block.
  int block_height_ = 0;

  // Pre-recorded Skia draw commands that draw a single watermark text block.
  // Since the block is repeated across the web page, this is more efficient
  // than repeated calls to `RenderText::Draw()`, which need to do additional
  // work such as loading font configs, shaping text, calculating line height,
  // etc. This only needs to be updated when the watermark text changes.
  WatermarkBlock watermark_block_;
};

}  // namespace enterprise_watermark

#endif  // CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_VIEW_H_
