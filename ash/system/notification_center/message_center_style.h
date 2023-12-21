// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_STYLE_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_STYLE_H_

#include "ash/system/tray/tray_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

namespace message_center_style {

constexpr SkColor kScrollShadowColor = SkColorSetARGB(0x24, 0x0, 0x0, 0x0);

// TODO(crbug.com/1309551): Get the colors from AshColorProvider once
// notification supports dark/light mode.
constexpr SkColor kSeparatorColor = SkColorSetA(SK_ColorBLACK, 0x24);  // 14%

constexpr int kEmptyIconSize = 24;
constexpr auto kEmptyIconPadding = gfx::Insets::TLBR(0, 0, 4, 0);

constexpr int kScrollShadowOffsetY = 2;
constexpr int kScrollShadowBlur = 2;

// Layout parameters for swipe control of notifications in message center.
constexpr int kSwipeControlButtonHorizontalMargin = 8;

constexpr int kMaxGroupedNotificationsInCollapsedState = 3;
constexpr auto kGroupedCollapsedCountViewInsets =
    gfx::Insets::TLBR(0, 0, 16, kTrayMenuWidth - 100);

}  // namespace message_center_style

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_STYLE_H_
