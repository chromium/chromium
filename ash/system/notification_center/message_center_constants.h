// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_CONSTANTS_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_CONSTANTS_H_

#include "chromeos/constants/chromeos_features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

namespace ash {

// View IDs
inline constexpr int kNotificationInlineSettingsCancelButton = 1101;
inline constexpr int kNotificationTurnOffNotificationsButton = 1102;

inline constexpr int kGroupedCollapsedSummaryLabelSpacing = 6;
inline constexpr int kGroupedCollapsedSummaryTitleLength = 150;
inline constexpr int kGroupedCollapsedSummaryMessageLength = 250;
inline constexpr auto kGroupedCollapsedSummaryInsets =
    gfx::Insets::TLBR(0, 50, 0, 16);

inline constexpr int kGroupedNotificationsExpandedSpacing = 0;
inline constexpr int kGroupedNotificationsCollapsedSpacing = 4;
inline constexpr auto kGroupedNotificationContainerCollapsedInsets =
    gfx::Insets::TLBR(0, 0, 20, 0);
inline constexpr auto kGroupedNotificationContainerExpandedInsets =
    gfx::Insets::TLBR(2, 0, 8, 0);

inline constexpr int kMessagePopupCornerRadius = 16;

inline constexpr int kMessageCenterNotificationInnerCornerRadius = 4;
inline constexpr int kMessageCenterScrollViewCornerRadius = 16;
inline constexpr int kMessageCenterPadding = 8;
inline constexpr int kMessageCenterBottomPadding = 8;
inline constexpr int kMessageListNotificationSpacing = 2;

inline constexpr auto kNotificationViewPadding = gfx::Insets(4);

inline constexpr auto kNotificationBarPadding = gfx::Insets::TLBR(8, 0, 0, 0);

// Horizontal spacing of the pill buttons inside notification.
inline constexpr int kNotificationPillButtonHorizontalSpacing = 12;

inline constexpr auto kNotificationSwipeControlPadding = gfx::Insets::VH(0, 20);

// Constants for notification views.
inline constexpr int kNotificationAppIconViewSize = 24;
inline constexpr int kNotificationAppIconImageSize = 16;
inline constexpr int kNotificationTitleLabelSize = 13;
inline constexpr int kNotificationMessageLabelSize = 12;
inline constexpr int kNotificationSecondaryLabelSize = 12;
inline constexpr int kNotificationControlButtonsHorizontalSpacing = 6;

// Bullet character. The divider symbol between the title and the timestamp.
inline constexpr char16_t kNotificationTitleRowDivider[] = u"\u2022";

// Target contrast ratio to reach when adjusting colors in dark mode.
inline constexpr float kDarkModeMinContrastRatio = 6.0;

// Constants for `ash_notification_view`.

// The width of notification that is displayed inside the message center.
// (Deprecated)
inline constexpr int kDeprecatedNotificationInMessageCenterWidth = 344;

// The width of notification that is displayed inside the message center.
inline constexpr int kNotificationInMessageCenterWidth = 384;

inline constexpr int kProgressBarWithActionButtonsBottomPadding = 16;
inline constexpr int kProgressBarExpandedBottomPadding =
    24 - kNotificationViewPadding.bottom();
inline constexpr int kProgressBarCollapsedBottomPadding =
    22 - kNotificationViewPadding.bottom();

inline constexpr auto kAppIconCollapsedPadding =
    gfx::Insets::TLBR(24, 12, 24, 0);
inline constexpr auto kAppIconExpandedPadding = gfx::Insets::TLBR(20, 12, 0, 0);

inline constexpr auto kExpandButtonCollapsedPadding =
    gfx::Insets::TLBR(4, 0, 0, 12);
inline constexpr auto kExpandButtonExpandedPadding =
    gfx::Insets::TLBR(0, 0, 0, 12);

inline constexpr auto kMessageLabelInExpandedStatePadding =
    gfx::Insets::TLBR(0, 0, 4, 12);
inline constexpr auto kMessageLabelInExpandedStateExtendedPadding =
    gfx::Insets::TLBR(0, 0, 20, 12);

inline constexpr int kControlButtonsContainerMinimumHeight = 20;
inline constexpr auto kControlButtonsContainerExpandedPadding =
    gfx::Insets::TLBR(6, 0, 2, 0);
inline constexpr auto kControlButtonsContainerCollapsedPadding =
    gfx::Insets::TLBR(2, 0, 0, 0);

// The minimum width necessary for the container that holds the expand/collapse
// button and the control buttons. This is calculated to ensure that there is
// 16dp of space between the expand/collapse button and the notification icon if
// there is one, or else 16dp between the expand/collapse button and the end of
// the notification title and/or message, if the title and/or message are long
// and are elided. See http://b/267195370 for details.
inline constexpr int kExpandAndControlButtonsContainerMinimumWidth = 52;

inline constexpr char kGoogleSansFont[] = "Google Sans";
inline constexpr int kHeaderViewLabelSize = 12;
inline constexpr char kNotificationBodyFontWeight = 13;

// Animation durations for children which are animated via LayerAnimations.
inline constexpr int kTitleRowTimestampFadeInAnimationDelayMs = 100;
inline constexpr int kTitleRowTimestampFadeInAnimationDurationMs = 100;
inline constexpr int kHeaderRowFadeInAnimationDelayMs = 50;
inline constexpr int kHeaderRowFadeInAnimationDurationMs = 150;
inline constexpr int kMessageLabelFadeInAnimationDelayMs = 100;
inline constexpr int kMessageLabelFadeInAnimationDurationMs = 100;
inline constexpr int kMessageLabelInExpandedStateFadeInAnimationDelayMs = 100;
inline constexpr int kMessageLabelInExpandedStateFadeInAnimationDurationMs =
    183;
inline constexpr int kActionsRowFadeInAnimationDelayMs = 50;
inline constexpr int kActionsRowFadeInAnimationDurationMs = 100;
inline constexpr int kActionButtonsFadeOutAnimationDurationMs = 100;
inline constexpr int kInlineReplyFadeInAnimationDurationMs = 100;
inline constexpr int kInlineReplyFadeOutAnimationDurationMs = 50;
inline constexpr int kLargeImageFadeInAnimationDelayMs = 50;
inline constexpr int kLargeImageFadeInAnimationDurationMs = 50;
inline constexpr int kLargeImageFadeOutAnimationDelayMs = 50;
inline constexpr int kLargeImageFadeOutAnimationDurationMs = 100;
inline constexpr int kLargeImageScaleAndTranslateDurationMs = 250;
inline constexpr int kLargeImageScaleDownDurationMs = 150;

inline constexpr int kCollapsedSummaryViewAnimationDurationMs = 50;
inline constexpr int kChildMainViewFadeInAnimationDurationMs = 100;
inline constexpr int kChildMainViewFadeOutAnimationDurationMs = 50;
inline constexpr int kExpandButtonFadeInLabelDelayMs = 50;
inline constexpr int kExpandButtonFadeInLabelDurationMs = 50;
inline constexpr int kExpandButtonFadeOutLabelDurationMs = 50;
inline constexpr int kExpandButtonShowLabelBoundsChangeDurationMs = 200;
inline constexpr int kExpandButtonHideLabelBoundsChangeDurationMs = 250;

// Animation durations for toggle inline settings in AshNotificationView.
inline constexpr int kToggleInlineSettingsFadeInDelayMs = 50;
inline constexpr int kToggleInlineSettingsFadeInDurationMs = 100;
inline constexpr int kToggleInlineSettingsFadeOutDurationMs = 50;

// Animation durations for converting from single to group notification.
inline constexpr int kConvertFromSingleToGroupFadeOutDurationMs = 66;
inline constexpr int kConvertFromSingleToGroupFadeInDurationMs = 100;
inline constexpr int kConvertFromSingleToGroupBoundsChangeDurationMs = 250;

// Animation durations for swiping notification to reveal controls.
inline constexpr int kNotificationSwipeControlFadeInDurationMs = 50;

// Animation durations for expand/collapse of MessageCenterView.
inline constexpr int kLargeImageExpandAndCollapseAnimationDuration = 300;
inline constexpr int kInlineReplyAndGroupedParentExpandAnimationDuration = 250;
inline constexpr int kInlineReplyAndGroupedParentCollapseAnimationDuration =
    200;
inline constexpr int kInlineSettingsExpandAndCollapseAnimationDuration = 200;
inline constexpr int kGeneralExpandAnimationDuration = 300;
inline constexpr int kGeneralCollapseAnimationDuration = 200;

// Animation durations for adding / removing grouped child views.
inline constexpr int kSlideOutGroupedNotificationAnimationDurationMs = 200;

// System notification notifier ids.
const char kLockScreenNotifierId[] = "ash.lockscreen_notification_controller";

// Returns the width of the notification in the message center.
inline int GetNotificationInMessageCenterWidth() {
  return chromeos::features::IsNotificationWidthIncreaseEnabled()
             ? kNotificationInMessageCenterWidth
             : kDeprecatedNotificationInMessageCenterWidth;
}

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_CONSTANTS_H_
