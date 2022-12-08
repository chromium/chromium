// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/icon_constants.h"

#include "ash/public/cpp/style/color_provider.h"

namespace app_list {

namespace {

constexpr SkColor kOmniboxGenericIconColor = gfx::kGoogleGrey700;

}  // namespace

SkColor GetGenericIconColor() {
  // May be null in tests.
  ash::ColorProvider* const color_provider = ash::ColorProvider::Get();
  if (color_provider) {
    return color_provider->GetContentLayerColor(
        ash::ColorProvider::ContentLayerType::kButtonIconColor);
  }
  return kOmniboxGenericIconColor;
}

}  // namespace app_list
