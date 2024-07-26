// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_STYLE_H_
#define ASH_PICKER_VIEWS_PICKER_STYLE_H_

#include "ash/style/system_shadow.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

inline constexpr int kPickerViewWidth = 320;

inline constexpr int kPickerContainerBorderRadius = 12;
inline constexpr ui::ColorId kPickerContainerBackgroundColor =
    cros_tokens::kCrosSysSystemBaseElevatedOpaque;
inline constexpr auto kPickerContainerShadowType =
    SystemShadow::Type::kElevation12;

enum class PickerLayoutType {
  kMainResultsBelowSearchField,
  kMainResultsAboveSearchField,
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_STYLE_H_
