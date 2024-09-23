// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_CONSTANTS_H_
#define ASH_SYSTEM_TRAY_TRAY_CONSTANTS_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

// Do not add constant colors in this file. Get the colors from AshColorProvider
// instead.

// The size delta between the default font and the font size found in tray
// items like labels and buttons.
inline constexpr int kTrayTextFontSizeIncrease = 2;

// Size of tray items on the primary axis.
inline constexpr int kTrayItemSize = 32;

inline constexpr float kTrayItemCornerRadius = kTrayItemSize / 2.f;

// The width of the tray menu.
inline constexpr int kTrayMenuWidth = 360;

// The wide width of the tray menu.
inline constexpr int kWideTrayMenuWidth = 400;

// Margins between the glanceable bubble and individual glanceables.
inline constexpr int kGlanceablesLeftRightMargin = 16;
inline constexpr int kGlanceablesVerticalMargin = 16;

inline constexpr base::TimeDelta kSecondaryBubbleDuration = base::Seconds(3);
inline constexpr base::TimeDelta kSecondaryBubbleWithSpokenFeedbackDuration =
    base::Seconds(5);

inline constexpr int kTrayPopupPaddingHorizontal = 18;
inline constexpr int kTrayPopupButtonEndMargin = 10;

// The standard padding used on the left and right of labels.
inline constexpr int kTrayPopupLabelHorizontalPadding = 4;

// The wide padding used on the left and right of labels.
inline constexpr int kWideTrayPopupLabelHorizontalPadding = 16;

// The padding used on the top and bottom of labels.
inline constexpr int kTrayPopupLabelVerticalPadding = 8;

// The minimum/default height of the rows in the system tray menu.
inline constexpr int kTrayPopupItemMinHeight = 48;

// The wide width used for the first region of the row (which holds an image).
inline constexpr int kWideTrayPopupItemMinStartWidth = 48;

// The width used for the first region of the row (an image).
inline constexpr int kTrayPopupItemMinStartWidth = 20;

// The size of the icons appearing in the material design system menu.
inline constexpr int kMenuIconSize = 20;

// The width used for the end region of the row (usually a more arrow).
inline constexpr int kTrayPopupItemMinEndWidth =
    kMenuIconSize + 2 * kTrayPopupButtonEndMargin;

// The wide width used for the end region of a row.
inline constexpr int kWideTrayPopupItemMinEndWidth = 20;

// Padding used on right side of labels to keep minimum distance to the next
// item. This applies to all labels in the system menu.
inline constexpr int kTrayPopupLabelRightPadding = 8;

// The width of ToggleButton views including any border padding.
inline constexpr int kTrayToggleButtonWidth = 68;

// Constants for the title row.
inline constexpr int kTitleRowProgressBarHeight = 2;

// Width of lines used to separate menu items (e.g. input method menu).
inline constexpr int kMenuSeparatorWidth = 1;

// The size of the icons appearing in the material design system tray.
inline constexpr int kTrayIconSize = 16;

// The padding around network tray icon in dip.
inline constexpr int kUnifiedTrayNetworkIconPadding = 4;

// The size of buttons in the system menu.
inline constexpr int kMenuButtonSize = 48;
// The vertical padding for the system menu separator.
inline constexpr int kMenuSeparatorVerticalPadding = 4;

// Additional margin on the left edge of the tray menu.
inline constexpr int kMenuExtraMarginFromLeftEdge = 4;

// Wide additional margin on the left edge of the quick settings.
inline constexpr int kWideMenuExtraMarginFromLeftEdge = 24;

// Default wide margin on right edge of the quick settings menu.
inline constexpr int kWideMenuExtraMarginsFromRightEdge = 18;

// The visual padding to the left of icons in the system menu.
inline constexpr int kMenuEdgeEffectivePadding =
    kMenuExtraMarginFromLeftEdge + (kMenuButtonSize - kMenuIconSize) / 2;

// The inset applied to clickable surfaces in the system menu that do not have
// the ink drop filling the entire bounds.
inline constexpr int kTrayPopupInkDropInset = 4;

// The radius used to draw the corners of the rounded rect style ink drops.
inline constexpr int kTrayPopupInkDropCornerRadius = 2;

// Threshold to ignore update on the slider value.
inline constexpr float kAudioSliderIgnoreUpdateThreshold = 0.01;

inline constexpr auto kUnifiedMenuItemPadding =
    gfx::Insets::TLBR(0, 16, 16, 16);
inline constexpr int kSliderChildrenViewSpacing = 8;

// Constants used in the QuickSettingsSlider of the `QuickSettingsView`.
inline constexpr int kQsSliderIconSize = 20;
inline constexpr auto kRadioSliderIconPadding = gfx::Insets::VH(0, 2);
inline constexpr auto kRadioSliderPadding = gfx::Insets::TLBR(4, 4, 4, 24);
inline constexpr auto kRadioSliderPreferredSize = gfx::Size(0, 44);
inline constexpr auto kRadioSliderViewPadding = gfx::Insets::TLBR(0, 20, 0, 0);

inline constexpr int kMessageCenterCollapseThreshold = 175;
inline constexpr int kStackedNotificationBarHeight = 32;
inline constexpr int kNotificationIconStackThreshold = 28;
inline constexpr int kUnifiedSliderViewSpacing = 12;
inline constexpr int kUnifiedMessageCenterBubbleSpacing = 8;
inline constexpr int kUnifiedNotificationCenterSpacing = 16;
inline constexpr int kUnifiedTrayBatteryIconSize = 20;
inline constexpr int kUnifiedTrayIconSize = 18;
inline constexpr int kUnifiedTrayNonRoundedSideRadius = 4;
inline constexpr int kUnifiedTraySubIconSize = 15;
inline constexpr int kUnifiedTrayTextTopPadding = 1;
inline constexpr int kUnifiedTrayTextRightPadding = 1;
inline constexpr int kUnifiedTrayTimeLeftPadding = 1;
inline constexpr int kUnifiedTraySpacingBetweenIcons = 6;
inline constexpr int kUnifiedTrayBatteryWidth = 12;
inline constexpr int kUnifiedTrayContentPadding = 12;
inline constexpr int kUnifiedTopShortcutSpacing = 16;
inline constexpr int kUnifiedNotificationHiddenLineHeight = 20;
inline constexpr int kUnifiedTopShortcutContainerTopPadding = 12;
inline constexpr int kUnifiedNotificationMinimumHeight = 40;
inline constexpr int kUnifiedBackButtonLeftPadding = 16;
inline constexpr auto kUnifiedTopShortcutPadding = gfx::Insets::VH(0, 16);
inline constexpr auto kUnifiedNotificationHiddenPadding =
    gfx::Insets::VH(6, 16);
inline constexpr int kUnifiedNotificationSeparatorThickness = 1;
inline constexpr gfx::Insets kTrayBackgroundFocusPadding(1);
inline constexpr gfx::Insets kUnifiedSystemInfoBatteryIconPadding =
    gfx::Insets::TLBR(2, 2, 2, 6);

// Size of an icon drawn inside top shortcut buttons.
// A dark disc with |kTrayItemSize| diameter is drawn in the background.
inline constexpr int kTrayTopShortcutButtonIconSize = 20;

inline constexpr int kUnifiedManagedDeviceSpacing = 8;
inline constexpr int kUnifiedSystemInfoHeight = 16;
inline constexpr int kUnifiedSystemInfoSpacing = 8;
inline constexpr gfx::Insets kUnifiedSystemInfoDateViewPadding(3);

// Constants used in StackedNotificationBar located on top of the message
// center.
inline constexpr auto kStackedNotificationIconsContainerPadding =
    gfx::Insets::TLBR(1, 14, 0, 8);
inline constexpr int kStackedNotificationBarMaxIcons = 3;
inline constexpr int kStackedNotificationBarIconSpacing = 6;
inline constexpr int kStackedNotificationIconSize = 18;
inline constexpr int kNotificationIconAnimationLowPosition = 7;
inline constexpr int kNotificationIconAnimationHighPosition = -3;
inline constexpr double kNotificationIconAnimationScaleFactor = 0.77;
inline constexpr int kNotificationIconAnimationUpDurationMs = 50;
inline constexpr int kNotificationIconAnimationDownDurationMs = 17;
inline constexpr int kNotificationIconAnimationOutDurationMs = 67;
inline constexpr double kNotificationCenterDragExpandThreshold = 0.8;

// Constants used in FeaturePodsView of UnifiedSystemTray.
inline constexpr gfx::Size kUnifiedFeaturePodSize(112, 94);
inline constexpr gfx::Size kUnifiedFeaturePodCollapsedSize(46, 46);
inline constexpr gfx::Insets kUnifiedFeaturePodHoverPadding(2);
inline constexpr int kUnifiedFeaturePodLabelWidth = 85;
inline constexpr int kUnifiedFeaturePodSpacing = 6;
inline constexpr int kUnifiedFeaturePodHoverCornerRadius = 4;
inline constexpr int kUnifiedFeaturePodVerticalPadding = 24;
inline constexpr int kUnifiedFeaturePodTopPadding = 20;
inline constexpr int kUnifiedFeaturePodBottomPadding = 0;
inline constexpr int kUnifiedFeaturePodHorizontalSidePadding = 12;
inline constexpr int kUnifiedFeaturePodHorizontalMiddlePadding = 0;
inline constexpr int kUnifiedFeaturePodCollapsedVerticalPadding = 12;
inline constexpr int kUnifiedFeaturePodCollapsedHorizontalPadding = 24;
inline constexpr int kUnifiedFeaturePodLabelLineHeight = 16;
inline constexpr int kUnifiedFeaturePodLabelMaxLines = 2;
inline constexpr int kUnifiedFeaturePodSubLabelLineHeight = 15;
inline constexpr int kUnifiedFeaturePodLabelFontSize = 13;
inline constexpr int kUnifiedFeaturePodSubLabelFontSize = 12;
inline constexpr int kUnifiedFeaturePodInterLabelPadding = 2;
inline constexpr int kUnifiedFeaturePodArrowSpacing = 4;
inline constexpr int kUnifiedFeaturePodMinimumHorizontalMargin = 4;
inline constexpr int kUnifiedFeaturePodItemsInRow = 3;
inline constexpr int kUnifiedFeaturePodMaxRows = 3;
inline constexpr int kUnifiedFeaturePodMinRows = 1;
inline constexpr int kUnifiedFeaturePodMaxItemsInCollapsed = 5;
inline constexpr int kUnifiedFeaturePodsPageSpacing = 48;

// Constants used in FeatureTiles of QuickSettingsView.
inline constexpr int kFeatureTileMaxRows = 4;
inline constexpr int kFeatureTileMaxRowsWhenMediaViewIsShowing = 3;
inline constexpr int kFeatureTileMinRows = 1;
inline constexpr int kPrimaryFeatureTileWidth = 180;
inline constexpr int kCompactFeatureTileWidth = 86;
inline constexpr int kFeatureTileHeight = 64;

// Constants used in system tray page transition animations.
inline constexpr double kCollapseThreshold = 0.3;

// Separators between multiple users are shorter than the full width.
inline constexpr int kUnifiedUserChooserSeparatorSideMargin = 64;
// Additional gap above and below the longer separator between user list and
// "Sign in another user..." button.
inline constexpr int kUnifiedUserChooserLargeSeparatorVerticalSpacing = 8;

inline constexpr int kUnifiedUserChooserRowHeight = 64;

// Gap between the buttons on the top shortcut row, other than the
// expand/collapse button.
inline constexpr int kUnifiedTopShortcutButtonDefaultSpacing = 16;
inline constexpr int kUnifiedTopShortcutButtonMinSpacing = 4;

// Constants used in the detailed view in UnifiedSystemTray.
inline constexpr int kUnifiedDetailedViewTitleRowHeight = 64;
inline constexpr int kTitleRightPadding = 16;
inline constexpr int kTitleItemBetweenSpacing = 8;
inline constexpr int kTitleRowProgressBarIndex = 1;

// Constants used for the status area overflow button and state.
inline constexpr gfx::Size kStatusAreaOverflowButtonSize(28, 32);
inline constexpr int kStatusAreaLeftPaddingForOverflow = 100;
inline constexpr int kStatusAreaForceCollapseAvailableWidth = 200;
inline constexpr int kStatusAreaOverflowGradientSize = 24;

// Height compensations in tablet mode based on whether the hotseat is shown.
inline constexpr int kTrayBubbleInsetTabletModeCompensation = 8;
inline constexpr int kTrayBubbleInsetHotseatCompensation = 16;

// Constants used for the privacy screen toast.
inline constexpr int kPrivacyScreenToastMinWidth = 256;
inline constexpr int kPrivacyScreenToastMaxWidth = 512;
inline constexpr int kPrivacyScreenToastHeight = 64;
inline constexpr int kPrivacyScreenToastMainLabelFontSize = 14;
inline constexpr int kPrivacyScreenToastSubLabelFontSize = 13;
inline constexpr auto kPrivacyScreenToastInsets = gfx::Insets::VH(10, 16);
inline constexpr int kPrivacyScreenToastSpacing = 16;

// Constants used for media tray.
inline constexpr int kMediaTrayPadding = 8;
inline constexpr int kMediaNotificationListViewBottomPadding = 8;

// There is no active user session during oobe, which means it doesn't support
// dark mode. Sets the icon color to be constant.
inline constexpr SkColor kIconColorInOobe = gfx::kGoogleGrey700;

// Constants used for the autozoom toast.
inline constexpr int kAutozoomToastMinWidth = 160;
inline constexpr int kAutozoomToastMaxWidth = 400;
inline constexpr int kAutozoomToastMainLabelFontSize = 14;
inline constexpr auto kAutozoomToastInsets = gfx::Insets::VH(10, 16);
inline constexpr int kAutozoomToastSpacing = 16;

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_CONSTANTS_H_
