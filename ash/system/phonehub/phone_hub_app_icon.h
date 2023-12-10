// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_ICON_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_ICON_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"

namespace ash {

class ASH_EXPORT AppIcon : public views::ImageView {
  METADATA_HEADER(AppIcon, views::ImageView)

 public:
  // Measured in DIPs.
  static constexpr int kSizeSmall = 20;
  static constexpr int kSizeNormal = 42;

  static constexpr gfx::Size GetRecommendedImageSize(int icon_size) {
    // Leave 1 DP of space around the image to avoid the appearance of clipping.
    constexpr int kRecommendedImageInset = 1;

    int length_minus_insets = icon_size - 2 * kRecommendedImageInset;
    return gfx::Size(length_minus_insets, length_minus_insets);
  }

  AppIcon(const gfx::Image& icon, int size);
  AppIcon(const AppIcon&) = delete;
  AppIcon& operator=(const AppIcon&) = delete;
  ~AppIcon() override = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_APP_ICON_H_
