// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_VIEW_H_
#define CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_VIEW_H_

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
  explicit WatermarkView(std::string text);
  ~WatermarkView() override;

  // Set this to a translucent value for testing. Useful for visualizing the
  // view's bounds when performing transformations.
  void SetBackgroundColor(SkColor color);

  // `text` must be UTF-8 encoded.
  void SetString(const std::string& text);

  // views::View
  void OnPaint(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  bool has_text_for_testing() const {
    return text_fill_.get() || text_outline_.get();
  }

 private:
  // Background color of the whole `WatermarkView`. This is normally
  // transparent, but can be an arbitrary color for testing with the
  // "watermark_app" target.
  SkColor background_color_;

  // Height required to draw all the lines in `text_fill_`/`text_outline_` in a
  // single block.
  int block_height_ = 0;

  // Containers for the fill/outline representations of a single text block.
  // This is done to avoid calling methods like `RenderText::SetText` as much as
  // possible as that would invalidate that object's layout cache.
  std::unique_ptr<gfx::RenderText> text_fill_;
  std::unique_ptr<gfx::RenderText> text_outline_;
};

}  // namespace enterprise_watermark

#endif  // CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_VIEW_H_
