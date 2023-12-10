// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_app_icon.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace ash {

AppIcon::AppIcon(const gfx::Image& icon, int size) {
  SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
      icon.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
      gfx::Size(size, size)));
}

BEGIN_METADATA(AppIcon)
END_METADATA

}  // namespace ash
