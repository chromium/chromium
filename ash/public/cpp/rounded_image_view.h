// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ROUNDED_IMAGE_VIEW_H_
#define ASH_PUBLIC_CPP_ROUNDED_IMAGE_VIEW_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace ash {

// A custom image view with rounded edges.
class ASH_PUBLIC_EXPORT RoundedImageView : public views::View {
 public:
  // Constructs a new rounded image view with rounded corners of radius
  // |corner_radius|.
  explicit RoundedImageView(int corner_radius);
  ~RoundedImageView() override;

  // Set the image that should be displayed. The image contents is copied to the
  // receiver's image.
  void SetImage(const gfx::ImageSkia& image);

  // Similar with the method above but the preferred image size is `size`.
  void SetImage(const gfx::ImageSkia& image, const gfx::Size& size);

  // Set the radii of the corners independently.
  void SetCornerRadii(int top_left,
                      int top_right,
                      int bottom_right,
                      int bottom_left);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

  const gfx::ImageSkia& original_image() const { return original_image_; }

 private:
  // Returns the preferred image size.
  gfx::Size GetImageSize() const;

  gfx::ImageSkia original_image_;
  gfx::ImageSkia resized_image_;
  int corner_radius_[4];

  DISALLOW_COPY_AND_ASSIGN(RoundedImageView);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ROUNDED_IMAGE_VIEW_H_
