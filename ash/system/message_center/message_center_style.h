// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

namespace message_center_style {

constexpr SkColor kEmptyViewColor = SkColorSetARGB(0x8A, 0x0, 0x0, 0x0);
constexpr SkColor kScrollShadowColor = SkColorSetARGB(0x24, 0x0, 0x0, 0x0);

constexpr int kEmptyIconSize = 24;
constexpr gfx::Insets kEmptyIconPadding(0, 0, 4, 0);

constexpr int kScrollShadowOffsetY = 2;
constexpr int kScrollShadowBlur = 2;

// Layout parameters for swipe control of notifications in message center.
constexpr int kSwipeControlButtonImageSize = 20;
constexpr int kSwipeControlButtonSize = 36;
constexpr int kSwipeControlButtonVerticalMargin = 24;
constexpr int kSwipeControlButtonHorizontalMargin = 8;
constexpr SkColor kSwipeControlBackgroundColor =
    SkColorSetRGB(0xee, 0xee, 0xee);

// The ratio to multiply with the swipe control width to get the width to
// display at full opacity when swiping.
constexpr float kSwipeControlFullOpacityRatio = 1.5f;

}  // namespace message_center_style

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
