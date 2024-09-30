// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_CORAL_GROUPED_ICON_IMAGE_H_
#define ASH_BIRCH_BIRCH_CORAL_GROUPED_ICON_IMAGE_H_

#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// TODO(http://b/280308773): Add pixel test for this class.
class CoralGroupedIconImage : public gfx::CanvasImageSource {
 public:
  CoralGroupedIconImage(const std::vector<gfx::ImageSkia>& icon_images,
                        int extra_number,
                        const ui::ColorProvider* color_provider);
  CoralGroupedIconImage(const CoralGroupedIconImage&) = delete;
  CoralGroupedIconImage& operator=(const CoralGroupedIconImage&) = delete;
  ~CoralGroupedIconImage() override;

  // Takes in a vector of `gfx::ImageSkia` icons and composes the grouped icon
  // image used in `BirchCoralItem` based on the number of icons.
  static ui::ImageModel DrawCoralGroupedIconImage(
      const std::vector<gfx::ImageSkia>& icons_images,
      int extra_number);

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

 private:
  // Represents the icon images (maximum of 4 non-unique icons) that will make
  // up the coral image.
  std::vector<gfx::ImageSkia> icon_images_;
  // Represents the number of extra tabs or apps that are part of the coral
  // grouping, will be painted as a label in the coral image.
  const int extra_number_;
  raw_ptr<const ui::ColorProvider> color_provider_;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_CORAL_GROUPED_ICON_IMAGE_H_
