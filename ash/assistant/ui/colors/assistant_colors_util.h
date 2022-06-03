// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_COLORS_ASSISTANT_COLORS_UTIL_H_
#define ASH_ASSISTANT_UI_COLORS_ASSISTANT_COLORS_UTIL_H_

#include "ash/assistant/ui/colors/assistant_colors.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {
namespace assistant {

// This redirects a request to assistant_colors::ResolveColor. If kDarkLightMode
// flag is off, this resolve the color from a map defined in the cc file.
SkColor ResolveAssistantColor(assistant_colors::ColorName color_name);

// Whether to use the modern dark/light mode colors, instead of legacy colors.
// Returns true if either feature kDarkLightMode or kProductivityLauncher is
// enabled.
bool UseDarkLightModeColors();

}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UI_COLORS_ASSISTANT_COLORS_UTIL_H_
