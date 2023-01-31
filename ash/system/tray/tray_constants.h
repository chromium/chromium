// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_CONSTANTS_H_
#define ASH_SYSTEM_TRAY_TRAY_CONSTANTS_H_

#include "ash/ash_export.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

// Do not add constant colors in this file. Get the colors from AshColorProvider
// instead.

// The size delta between the default font and the font size found in tray
// items like labels and buttons.
constexpr int kTrayTextFontSizeIncrease = 2;

// Size of tray items on the primary axis.
constexpr int kTrayItemSize = 32;

constexpr float kTrayItemCornerRadius = kTrayItemSize / 2.f;

// The width of the tray menu.
constexpr int kTrayMenuWidth = 360;

// The width of the revamped tray menu.
constexpr int kRevampedTrayMenuWidth = 440;

// TODO(b/258072559): Update this height once we have finalized UX specs for
// tray height.
// The maximum height of the revamped tray menu.
constexpr int kRevampedTrayMenuMaxHeight = 508;

constexpr int kTrayPopupAutoCloseDelayInSeconds = 2;
constexpr int kTrayPopupAutoCloseDelayInSecondsWithSpokenFeedback = 5;
constexpr int kTrayPopupPaddingHorizontal = 18;
constexpr int kTrayPopupButtonEndMargin = 10;

// The padding used on the left and right of labels. This applies to all labels
// in the system menu.
constexpr int kTrayPopupLabelHorizontalPadding = 4;

// The padding used on the left and right of labels with QsRevamp.
constexpr int kQsPopupLabelHorizontalPadding = 16;

// The padding used on the top and bottom of labels.
constexpr int kTrayPopupLabelVerticalPadding = 8;

// The minimum/default height of the rows in the system tray menu.
constexpr int kTrayPopupItemMinHeight = 48;

// The width used for the first region of the row (which holds an image).
constexpr int kTrayPopupItemMinStartWidth = 48;

// The width used for the first region of the row (an image) with QsRevamp.
constexpr int kQsPopupItemMinStartWidth = 20;

// The size of the icons appearing in the material design system menu.
constexpr int kMenuIconSize = 20;

// The width used for the end region of the row (usually a more arrow).
constexpr int kTrayPopupItemMinEndWidth =
    kMenuIconSize + 2 * kTrayPopupButtonEndMargin;

// The width used for the end region of a row with QsRevamp.
constexpr int kQsPopupItemMinEndWidth = 20;

// Padding used on right side of labels to keep minimum distance to the next
// item. This applies to all labels in the system menu.
constexpr int kTrayPopupLabelRightPadding = 8;

// The width of ToggleButton views including any border padding.
constexpr int kTrayToggleButtonWidth = 68;

// Constants for the title row.
constexpr int kTitleRowProgressBarHeight = 2;

// Width of lines used to separate menu items (e.g. input method menu).
constexpr int kMenuSeparatorWidth = 1;

// Width of lines used to separate sections of the system tray, for instance
// in tray detailed views.
constexpr int kTraySeparatorWidth = 0;

// The size of the icons appearing in the material design system tray.
constexpr int kTrayIconSize = 16;

// The padding around network tray icon in dip.
constexpr int kUnifiedTrayNetworkIconPadding = 4;

// The size of buttons in the system menu.
constexpr int kMenuButtonSize = 48;
// The vertical padding for the system menu separator.
constexpr int kMenuSeparatorVerticalPadding = 4;

// Additional margin on the left edge of the tray menu.
constexpr int kMenuExtraMarginFromLeftEdge = 4;

// Additional margin on the left edge of the quick settings menu with QsRevamp.
constexpr int kQsExtraMarginFromLeftEdge = 24;

// Default margin on right edge of the quick settings menu with QsRevamp.
constexpr int kQsExtraMarginsFromRightEdge = 18;

// The visual padding to the left of icons in the system menu.
constexpr int kMenuEdgeEffectivePadding =
    kMenuExtraMarginFromLeftEdge + (kMenuButtonSize - kMenuIconSize) / 2;

// The inset applied to clickable surfaces in the system menu that do not have
// the ink drop filling the entire bounds.
constexpr int kTrayPopupInkDropInset = 4;

// The radius used to draw the corners of the rounded rect style ink drops.
constexpr int kTrayPopupInkDropCornerRadius = 2;

// Threshold to ignore update on the slider value.
constexpr float kAudioSliderIgnoreUpdateThreshold = 0.01;

// Duration for the collapse / expand animation in ms.
constexpr int kSystemMenuCollapseExpandAnimationDurationMs = 500;

constexpr auto kUnifiedMenuItemPadding = gfx::Insets::TLBR(0, 16, 16, 16);
constexpr auto kUnifiedSystemInfoViewPadding = gfx::Insets::TLBR(0, 16, 16, 16);
constexpr auto kUnifiedSliderRowPadding = gfx::Insets::TLBR(0, 12, 8, 16);
constexpr auto kUnifiedSliderBubblePadding = gfx::Insets::TLBR(12, 0, 4, 0);
constexpr auto kUnifiedSliderPadding = gfx::Insets::VH(0, 16);
constexpr auto kMicGainSliderViewPadding = gfx::Insets::TLBR(0, 52, 8, 0);
constexpr auto kMicGainSliderPadding = gfx::Insets::TLBR(0, 8, 0, 48);

// Constants used in the QuickSettingsSlider of the `QuickSettingsView`.
constexpr int kQsSliderIconSize = 20;
constexpr auto kRadioSliderIconPadding = gfx::Insets::VH(0, 2);
constexpr auto kRadioSliderPadding = gfx::Insets::TLBR(0, 4, 0, 24);
constexpr auto kRadioSliderPreferredSize = gfx::Size(0, 44);
constexpr auto kRadioSliderViewPadding = gfx::Insets::TLBR(0, 20, 0, 0);
constexpr int kRadioSliderViewSpacing = 8;

constexpr int kMessageCenterCollapseThreshold = 175;
constexpr int kStackedNotificationBarHeight = 32;
constexpr int kStackedNotificationBarCollapsedHeight = 40;
constexpr int kNotificationIconStackThreshold = 28;
constexpr int kUnifiedSliderViewSpacing = 12;
constexpr int kUnifiedMessageCenterBubbleSpacing = 8;
constexpr int kUnifiedNotificationCenterSpacing = 16;
constexpr int kUnifiedTrayBatteryIconSize = 20;
constexpr int kUnifiedTrayIconSize = 18;
constexpr int kUnifiedTrayNonRoundedSideRadius = 4;
constexpr int kUnifiedTraySubIconSize = 15;
constexpr int kUnifiedTrayTextTopPadding = 1;
constexpr int kUnifiedTrayTextRightPadding = 1;
constexpr int kUnifiedTrayTimeLeftPadding = 1;
constexpr int kUnifiedTraySpacingBetweenIcons = 6;
constexpr int kUnifiedTrayBatteryWidth = 12;
constexpr int kUnifiedTrayContentPadding = 12;
constexpr int kUnifiedTopShortcutSpacing = 16;
constexpr int kUnifiedNotificationHiddenLineHeight = 20;
constexpr int kUnifiedTopShortcutContainerTopPadding = 12;
constexpr int kUnifiedNotificationMinimumHeight = 40;
constexpr int kUnifiedBackButtonLeftPadding = 16;
constexpr auto kUnifiedTopShortcutPadding = gfx::Insets::VH(0, 16);
constexpr auto kUnifiedNotificationHiddenPadding = gfx::Insets::VH(6, 16);
constexpr int kUnifiedNotificationSeparatorThickness = 1;
constexpr gfx::Insets kUnifiedCircularButtonFocusPadding(4);
constexpr gfx::Insets kTrayBackgroundFocusPadding(1);
constexpr gfx::Insets kUnifiedSystemInfoBatteryIconPadding =
    gfx::Insets::TLBR(2, 2, 2, 6);

// Size of an icon drawn inside top shortcut buttons.
// A dark disc with |kTrayItemSize| diameter is drawn in the background.
constexpr int kTrayTopShortcutButtonIconSize = 20;

constexpr int kUnifiedManagedDeviceSpacing = 8;
constexpr int kUnifiedSystemInfoHeight = 16;
constexpr int kUnifiedSystemInfoSpacing = 8;
constexpr gfx::Insets kUnifiedSystemInfoDateViewPadding(3);

// Constants used in StackedNotificationBar located on top of the message
// center.
constexpr auto kStackedNotificationIconsContainerPadding =
    gfx::Insets::TLBR(1, 14, 0, 8);
constexpr int kStackedNotificationBarMaxIcons = 3;
constexpr int kStackedNotificationBarIconSpacing = 6;
constexpr int kStackedNotificationIconSize = 18;
constexpr int kNotificationIconAnimationLowPosition = 7;
constexpr int kNotificationIconAnimationHighPosition = -3;
constexpr double kNotificationIconAnimationScaleFactor = 0.77;
constexpr int kNotificationIconAnimationUpDurationMs = 50;
constexpr int kNotificationIconAnimationDownDurationMs = 17;
constexpr int kNotificationIconAnimationOutDurationMs = 67;
constexpr double kNotificationCenterDragExpandThreshold = 0.8;

// Constants used in FeaturePodsView of UnifiedSystemTray.
constexpr gfx::Size kUnifiedFeaturePodSize(112, 94);
constexpr gfx::Size kUnifiedFeaturePodCollapsedSize(46, 46);
constexpr gfx::Insets kUnifiedFeaturePodHoverPadding(2);
constexpr int kUnifiedFeaturePodLabelWidth = 85;
constexpr int kUnifiedFeaturePodSpacing = 6;
constexpr int kUnifiedFeaturePodHoverCornerRadius = 4;
constexpr int kUnifiedFeaturePodVerticalPadding = 24;
constexpr int kUnifiedFeaturePodTopPadding = 20;
constexpr int kUnifiedFeaturePodBottomPadding = 0;
constexpr int kUnifiedFeaturePodHorizontalSidePadding = 12;
constexpr int kUnifiedFeaturePodHorizontalMiddlePadding = 0;
constexpr int kUnifiedFeaturePodCollapsedVerticalPadding = 12;
constexpr int kUnifiedFeaturePodCollapsedHorizontalPadding = 24;
constexpr int kUnifiedFeaturePodLabelLineHeight = 16;
constexpr int kUnifiedFeaturePodLabelMaxLines = 2;
constexpr int kUnifiedFeaturePodSubLabelLineHeight = 15;
constexpr int kUnifiedFeaturePodLabelFontSize = 13;
constexpr int kUnifiedFeaturePodSubLabelFontSize = 12;
constexpr int kUnifiedFeaturePodInterLabelPadding = 2;
constexpr int kUnifiedFeaturePodArrowSpacing = 4;
constexpr int kUnifiedFeaturePodMinimumHorizontalMargin = 4;
constexpr int kUnifiedFeaturePodItemsInRow = 3;
constexpr int kUnifiedFeaturePodMaxRows = 3;
constexpr int kUnifiedFeaturePodMinRows = 1;
constexpr int kUnifiedFeaturePodMaxItemsInCollapsed = 5;
constexpr int kUnifiedFeaturePodsPageSpacing = 48;

// Constants used in FeatureTiles of QuickSettingsView.
constexpr int kFeatureTileMaxRows = 4;
constexpr int kFeatureTileMinRows = 1;
constexpr int kFeatureTileHeight = 64;

// Height of the page indicator view.
constexpr int kPageIndicatorViewMaxHeight = 20;

// Constants used in system tray page transition animations.
constexpr double kCollapseThreshold = 0.3;

// Separators between multiple users are shorter than the full width.
constexpr int kUnifiedUserChooserSeparatorSideMargin = 64;
// Additional gap above and below the longer separator between user list and
// "Sign in another user..." button.
constexpr int kUnifiedUserChooserLargeSeparatorVerticalSpacing = 8;

constexpr int kUnifiedUserChooserRowHeight = 64;

// Gap between the buttons on the top shortcut row, other than the
// expand/collapse button.
constexpr int kUnifiedTopShortcutButtonDefaultSpacing = 16;
constexpr int kUnifiedTopShortcutButtonMinSpacing = 4;

// Constants used in the detailed view in UnifiedSystemTray.
constexpr auto kUnifiedDetailedViewTitlePadding =
    gfx::Insets::TLBR(0, 0, 0, 16);
constexpr int kUnifiedDetailedViewTitleRowHeight = 64;

// Constants used for the status area overflow button and state.
constexpr gfx::Size kStatusAreaOverflowButtonSize(28, 32);
constexpr int kStatusAreaLeftPaddingForOverflow = 100;
constexpr int kStatusAreaForceCollapseAvailableWidth = 200;
constexpr int kStatusAreaOverflowGradientSize = 24;

// Height compensations in tablet mode based on whether the hotseat is shown.
constexpr int kTrayBubbleInsetTabletModeCompensation = 8;
constexpr int kTrayBubbleInsetHotseatCompensation = 16;

// Constants used for the privacy screen toast.
constexpr int kPrivacyScreenToastMinWidth = 256;
constexpr int kPrivacyScreenToastMaxWidth = 512;
constexpr int kPrivacyScreenToastHeight = 64;
constexpr int kPrivacyScreenToastMainLabelFontSize = 14;
constexpr int kPrivacyScreenToastSubLabelFontSize = 13;
constexpr auto kPrivacyScreenToastInsets = gfx::Insets::VH(10, 16);
constexpr int kPrivacyScreenToastSpacing = 16;

// Constants used for media tray.
constexpr int kMediaTrayPadding = 8;

// There is no active user session during oobe, which means it doesn't support
// dark mode. Sets the icon color to be constant.
constexpr SkColor kIconColorInOobe = gfx::kGoogleGrey700;

// Constants used for the autozoom toast.
constexpr int kAutozoomToastMinWidth = 160;
constexpr int kAutozoomToastMaxWidth = 400;
constexpr int kAutozoomToastMainLabelFontSize = 14;
constexpr auto kAutozoomToastInsets = gfx::Insets::VH(10, 16);
constexpr int kAutozoomToastSpacing = 16;

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_CONSTANTS_H_
