// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ASH_COLOR_ID_H_
#define ASH_STYLE_ASH_COLOR_ID_H_

#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"

namespace ash {

// clang-format off
#define ASH_COLOR_IDS \
  /* Shield and Base layer colors. */ \
  E_CPONLY(kColorAshShieldAndBase20) \
  E_CPONLY(kColorAshShieldAndBase40) \
  E_CPONLY(kColorAshShieldAndBase60) \
  E_CPONLY(kColorAshShieldAndBase80) \
  E_CPONLY(kColorAshShieldAndBase90) \
  E_CPONLY(kColorAshShieldAndBase95) \
  E_CPONLY(kColorAshShieldAndBaseOpaque) \
  E_CPONLY(kColorAshHairlineBorderColor) \
  E_CPONLY(kColorAshControlBackgroundColorActive) \
  E_CPONLY(kColorAshControlBackgroundColorAlert) \
  E_CPONLY(kColorAshControlBackgroundColorInactive) \
  E_CPONLY(kColorAshControlBackgroundColorWarning) \
  E_CPONLY(kColorAshControlBackgroundColorPositive) \
  E_CPONLY(kColorAshFocusAuraColor)

#include "ui/color/color_id_macros.inc"

enum AshColorIds : ui::ColorId {
  kAshColorsStart = cros_tokens::kCrosSysColorsEnd,

  ASH_COLOR_IDS

  kAshColorsEnd,
};

#include "ui/color/color_id_macros.inc"

// clang-format on

}  // namespace ash

#endif  // ASH_STYLE_ASH_COLOR_ID_H_
