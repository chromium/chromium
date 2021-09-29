// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONSTANTS_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONSTANTS_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

constexpr int kMessagePopupCornerRadius = 16;
constexpr int kMessageCenterNotificationCornerRadius = 2;

constexpr int kMessageCenterSidePadding = 8;
constexpr int kMessageCenterBottomPadding = 8;
constexpr int kMessageListNotificationSpacing = 2;

constexpr int kNotificationBarVerticalPadding = 8;
constexpr int kNotificationBarHorizontalPadding = 16;

constexpr gfx::Insets kAppIconViewExpandedPadding(2, 0, 0, 0);
constexpr gfx::Insets kAppIconViewCollapsedPadding(6, 0, 0, 0);

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONSTANTS_H_
