// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ARROW_CONTAINER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ARROW_CONTAINER_H_

#include <memory>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
class Size;
}  // namespace gfx

namespace arc::input_overlay {

// ArrowContainer is a container with an arrow on left or right side.
class ArrowContainer : public views::View {
  METADATA_HEADER(ArrowContainer, views::View)

 public:
  ArrowContainer();
  ArrowContainer(const ArrowContainer&) = delete;
  ArrowContainer& operator=(const ArrowContainer&) = delete;
  ~ArrowContainer() override;

  // Set triangle wedge offset from center of the container on the height.
  //   - `offset` < 0, the triangle wedge is above the center.
  //   - `offset` > 0, the triangle wedge is below the center.
  //   - `offset` = 0, the triangle wedge is right on the center.
  void SetArrowVerticalOffset(int offset);
  // Set the triangle wedge on left or right side.
  void SetArrowOnLeft(bool arrow_on_left);

 private:
  class ShadowLayer;

  void UpdateBorder();

  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  std::unique_ptr<ShadowLayer> shadow_layer_;

  int arrow_vertical_offset_ = 0;
  bool arrow_on_left_ = false;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ARROW_CONTAINER_H_
