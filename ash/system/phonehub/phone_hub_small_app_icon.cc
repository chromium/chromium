// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_small_app_icon.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

// Appearance in DIPs.
constexpr int kSmallAppIconSize = 20;

}  // namespace

namespace ash {

SmallAppIcon::SmallAppIcon(const gfx::Image& icon) {
  SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
      icon.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kSmallAppIconSize, kSmallAppIconSize)));
}

// views::View:
const char* SmallAppIcon::GetClassName() const {
  return "SmallAppIcon";
}

}  // namespace ash