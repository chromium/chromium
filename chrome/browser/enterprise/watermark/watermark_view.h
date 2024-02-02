// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_VIEW_H_
#define CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

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

  // views::View
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  std::string text_;
};

}  // namespace enterprise_watermark

#endif
