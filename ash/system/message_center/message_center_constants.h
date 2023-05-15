// Copyright 2021 The Chromium Authors
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
constexpr auto kGroupedCollapsedSummaryInsets = gfx::Insets::TLBR(0, 50, 0, 16);

constexpr int kGroupedNotificationsExpandedSpacing = 0;
constexpr int kGroupedNotificationsCollapsedSpacing = 4;
constexpr auto kGroupedNotificationContainerCollapsedInsets =
    gfx::Insets::TLBR(0, 0, 20, 0);
constexpr auto kGroupedNotificationContainerExpandedInsets =
    gfx::Insets::TLBR(2, 0, 8, 0);

constexpr int kMessagePopupCornerRadius = 16;

constexpr int kMessageCenterNotificationInnerCornerRadius = 2;
constexpr int kMessageCenterScrollViewCornerRadius = 12;
constexpr int kJellyMessageCenterNotificationInnerCornerRadius = 4;
constexpr int kJellyMessageCenterScrollViewCornerRadius = 16;
constexpr int kMessageCenterPadding = 8;
constexpr int kMessageCenterBottomPadding = 8;
constexpr int kMessageListNotificationSpacing = 2;

constexpr auto kNotificationViewPadding = gfx::Insets(4);

constexpr auto kNotificationBarPadding = gfx::Insets::TLBR(8, 0, 0, 4);

// Horizontal spacing of the pill buttons inside notification.
constexpr int kNotificationPillButtonHorizontalSpacing = 12;

constexpr auto kNotificationSwipeControlPadding = gfx::Insets::VH(0, 20);

// Constants for `ash_notification_view`.

// The width of notification that displayed inside the message center.
constexpr int kNotificationInMessageCenterWidth = 344;

constexpr int kProgressBarWithActionButtonsBottomPadding = 16;
constexpr int kProgressBarExpandedBottomPadding =
    24 - kNotificationViewPadding.bottom();
constexpr int kProgressBarCollapsedBottomPadding =
    22 - kNotificationViewPadding.bottom();

constexpr auto kAppIconCollapsedPadding = gfx::Insets::TLBR(24, 12, 24, 0);
constexpr auto kAppIconExpandedPadding = gfx::Insets::TLBR(20, 12, 0, 0);

constexpr auto kExpandButtonCollapsedPadding = gfx::Insets::TLBR(4, 0, 0, 12);
constexpr auto kExpandButtonExpandedPadding = gfx::Insets::TLBR(0, 0, 0, 12);

constexpr auto kMessageLabelInExpandedStatePadding =
    gfx::Insets::TLBR(0, 0, 4, 12);
constexpr auto kMessageLabelInExpandedStateExtendedPadding =
    gfx::Insets::TLBR(0, 0, 20, 12);

constexpr int kControlButtonsContainerMinimumHeight = 20;
constexpr auto kControlButtonsContainerExpandedPadding =
    gfx::Insets::TLBR(6, 0, 2, 0);
constexpr auto kControlButtonsContainerCollapsedPadding =
    gfx::Insets::TLBR(2, 0, 0, 0);

constexpr char kGoogleSansFont[] = "Google Sans";
constexpr int kHeaderViewLabelSize = 12;
constexpr char kNotificationBodyFontWeight = 13;

// Animation durations for children which are animated via LayerAnimations.
constexpr int kTitleRowTimestampFadeInAnimationDelayMs = 100;
constexpr int kTitleRowTimestampFadeInAnimationDurationMs = 100;
constexpr int kHeaderRowFadeInAnimationDelayMs = 50;
constexpr int kHeaderRowFadeInAnimationDurationMs = 150;
constexpr int kMessageLabelFadeInAnimationDelayMs = 100;
constexpr int kMessageLabelFadeInAnimationDurationMs = 100;
constexpr int kMessageLabelInExpandedStateFadeInAnimationDelayMs = 100;
constexpr int kMessageLabelInExpandedStateFadeInAnimationDurationMs = 183;
constexpr int kActionsRowFadeInAnimationDelayMs = 50;
constexpr int kActionsRowFadeInAnimationDurationMs = 100;
constexpr int kActionButtonsFadeOutAnimationDurationMs = 100;
constexpr int kInlineReplyFadeInAnimationDurationMs = 100;
constexpr int kInlineReplyFadeOutAnimationDurationMs = 50;
constexpr int kLargeImageFadeInAnimationDelayMs = 50;
constexpr int kLargeImageFadeInAnimationDurationMs = 50;
constexpr int kLargeImageFadeOutAnimationDelayMs = 50;
constexpr int kLargeImageFadeOutAnimationDurationMs = 100;
constexpr int kLargeImageScaleAndTranslateDurationMs = 250;
constexpr int kLargeImageScaleDownDurationMs = 150;

constexpr int kCollapsedSummaryViewAnimationDurationMs = 50;
constexpr int kChildMainViewFadeInAnimationDurationMs = 100;
constexpr int kChildMainViewFadeOutAnimationDurationMs = 50;
constexpr int kExpandButtonFadeInLabelDelayMs = 50;
constexpr int kExpandButtonFadeInLabelDurationMs = 50;
constexpr int kExpandButtonFadeOutLabelDurationMs = 50;
constexpr int kExpandButtonShowLabelBoundsChangeDurationMs = 200;
constexpr int kExpandButtonHideLabelBoundsChangeDurationMs = 250;

// Animation durations for toggle inline settings in AshNotificationView.
constexpr int kToggleInlineSettingsFadeInDelayMs = 50;
constexpr int kToggleInlineSettingsFadeInDurationMs = 100;
constexpr int kToggleInlineSettingsFadeOutDurationMs = 50;

// Animation durations for converting from single to group notification.
constexpr int kConvertFromSingleToGroupFadeOutDurationMs = 66;
constexpr int kConvertFromSingleToGroupFadeInDurationMs = 100;
constexpr int kConvertFromSingleToGroupBoundsChangeDurationMs = 250;

// Animation durations for swiping notification to reveal controls.
constexpr int kNotificationSwipeControlFadeInDurationMs = 50;

// Animation durations for expand/collapse of MessageCenterView.
constexpr int kLargeImageExpandAndCollapseAnimationDuration = 300;
constexpr int kInlineReplyAndGroupedParentExpandAnimationDuration = 250;
constexpr int kInlineReplyAndGroupedParentCollapseAnimationDuration = 200;
constexpr int kInlineSettingsExpandAndCollapseAnimationDuration = 200;
constexpr int kGeneralExpandAnimationDuration = 300;
constexpr int kGeneralCollapseAnimationDuration = 200;

// Animation durations for adding / removing grouped child views.
constexpr int kSlideOutGroupedNotificationAnimationDurationMs = 200;

// System notification notifier ids.
const char kLockScreenNotifierId[] = "ash.lockscreen_notification_controller";

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONSTANTS_H_
