// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONSTANTS_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONSTANTS_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

constexpr int kGroupedCollapsedSummaryLabelSpacing = 6;
constexpr int kGroupedCollapsedSummaryTitleLength = 150;
constexpr int kGroupedCollapsedSummaryMessageLength = 250;
constexpr gfx::Insets kGroupedCollapsedSummaryInsets(0, 40, 0, 16);

constexpr int kGroupedNotificationsExpandedSpacing = 20;
constexpr int kGroupedNotificationsCollapsedSpacing = 6;
constexpr gfx::Insets kGroupedNotificationContainerInsets(8, 0);

constexpr int kMessagePopupCornerRadius = 16;

constexpr int kMessageCenterNotificationCornerRadius = 2;
constexpr int kMessageCenterScrollViewCornerRadius = 12;
constexpr int kMessageCenterSidePadding = 8;
constexpr int kMessageCenterBottomPadding = 8;
constexpr int kMessageListNotificationSpacing = 2;

constexpr int kNotificationBarVerticalPadding = 8;
constexpr int kNotificationBarHorizontalPadding = 10;

// Constants for `ash_notification_view`.

// The width of notification that displayed inside the message center.
constexpr int kNotificationInMessageCenterWidth = 344;

constexpr gfx::Insets kNotificationExpandButtonInsets(4, 4);
constexpr int kNotificationExpandButtonChildSpacing = 4;
constexpr int kNotificationExpandButtonCornerRadius = 12;
constexpr int kNotificationExpandButtonChevronIconSize = 16;
constexpr int kNotificationExpandButtonLabelFontSize = 12;
constexpr gfx::Size kNotificationExpandButtonLabelSize(8, 16);
constexpr gfx::Size kNotificationExpandButtonSize(24, 24);
constexpr gfx::Size kNotificationExpandButtonWithLabelSize(40, 24);

constexpr gfx::Insets kAppIconExpandButtonExpandedPadding(2, 0, 0, 0);
constexpr gfx::Insets kAppIconExpandButtonCollapsedPadding(12, 0, 0, 0);

constexpr gfx::Insets kControlButtonsContainerExpandedPadding(6, 0, 2, 0);
constexpr gfx::Insets kControlButtonsContainerCollapsedPadding(2, 0, 0, 0);

constexpr char kGoogleSansFont[] = "Google Sans";
constexpr int kHeaderViewLabelSize = 12;
constexpr char kNotificationBodyFontWeight = 13;

// Animation durations for children which are animated via LayerAnimations.
constexpr int kTitleRowTimestampFadeInAnimationDelayMs = 100;
constexpr int kTitleRowTimestampFadeInAnimationDurationMs = 100;
constexpr int kHeaderRowFadeInAnimationDelayMs = 50;
constexpr int kHeaderRowFadeInAnimationDurationMs = 150;
constexpr int kMessageViewFadeInAnimationDelayMs = 100;
constexpr int kMessageViewFadeInAnimationDurationMs = 100;
constexpr int kMessageViewInExpandedStateFadeInAnimationDelayMs = 100;
constexpr int kMessageViewInExpandedStateFadeInAnimationDurationMs = 183;
constexpr int kActionsRowFadeInAnimationDelayMs = 50;
constexpr int kActionsRowFadeInAnimationDurationMs = 100;
constexpr int kActionButtonsFadeOutAnimationDurationMs = 100;
constexpr int kInlineReplyFadeInAnimationDurationMs = 100;
constexpr int kInlineReplyFadeOutAnimationDurationMs = 50;

constexpr int kCollapsedSummaryViewAnimationDurationMs = 50;
constexpr int kChildMainViewFadeInAnimationDurationMs = 100;
constexpr int kChildMainViewFadeOutAnimationDurationMs = 50;
constexpr int kExpandButtonFadeInLabelDelayMs = 50;
constexpr int kExpandButtonFadeInLabelDurationMs = 50;
constexpr int kExpandButtonFadeOutLabelDurationMs = 50;
constexpr int kExpandButtonShowLabelBoundsChangeDurationMs = 200;
constexpr int kExpandButtonHideLabelBoundsChangeDurationMs = 250;

// Animation durations for expand/collapse of MessageCenterView.
constexpr int kLargeImageExpandAndCollapseAnimationDuration = 300;
constexpr int kInlineReplyAndGroupedParentExpandAnimationDuration = 250;
constexpr int kInlineReplyAndGroupedParentCollapseAnimationDuration = 200;
constexpr int kInlineSettingsExpandAndCollapseAnimationDuration = 200;
constexpr int kGeneralExpandAnimationDuration = 300;
constexpr int kGeneralCollapseAnimationDuration = 200;

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONSTANTS_H_
