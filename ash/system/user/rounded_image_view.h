// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_ROUNDED_IMAGE_VIEW_H_
#define ASH_SYSTEM_USER_ROUNDED_IMAGE_VIEW_H_

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace ash {
namespace tray {

// A custom image view with rounded edges.
class RoundedImageView : public views::View {
 public:
  // Constructs a new rounded image view with rounded corners of radius
  // |corner_radius|.
  explicit RoundedImageView(int corner_radius);
  ~RoundedImageView() override;

  // Set the image that should be displayed. The image contents is copied to the
  // receiver's image.
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

  // Gets the image painted by RoundedImageView for test.
  const gfx::ImageSkia& image_for_test() const { return resized_image_; }

 private:
  gfx::ImageSkia resized_image_;
  gfx::Size image_size_;
  int corner_radius_[4];

  DISALLOW_COPY_AND_ASSIGN(RoundedImageView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_USER_ROUNDED_IMAGE_VIEW_H_
