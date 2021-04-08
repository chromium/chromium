// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ui/border_factory.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"

namespace ui {
namespace ime {

std::unique_ptr<views::BubbleBorder> GetBorderForWindow(
    WindowBorderType windowType) {
  std::unique_ptr<views::BubbleBorder> border;
  switch (windowType) {
    // Currently all cases are the same, but they may become different later.
    case WindowBorderType::Undo:
    case WindowBorderType::Suggestion:
    default:
      border = std::make_unique<views::BubbleBorder>(
          views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW,
          gfx::kPlaceholderColor);
      border->set_md_shadow_elevation(
          ChromeLayoutProvider::Get()->GetShadowElevationMetric(
              views::Emphasis::kMedium));
  }
  border->SetCornerRadius(views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMedium));
  border->set_use_theme_background_color(true);
  return border;
}

}  // namespace ime
}  // namespace ui
