// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
extern const int kTrayTextFontSizeIncrease;

ASH_EXPORT extern const int kTrayItemSize;

extern const float kTrayItemCornerRadius;

// Extra padding used beside a single icon in the tray area of the shelf.
constexpr int kTrayImageItemPadding = 3;

// The width of the tray menu.
extern const int kTrayMenuWidth;

extern const int kTrayPopupAutoCloseDelayInSeconds;
extern const int kTrayPopupAutoCloseDelayInSecondsWithSpokenFeedback;
extern const int kTrayPopupPaddingHorizontal;
extern const int kTrayPopupButtonEndMargin;

// The padding used on the left and right of labels. This applies to all labels
// in the system menu.
extern const int kTrayPopupLabelHorizontalPadding;

// The horizontal padding used to properly lay out a slider in a TriView
// container with a FillLayout (such as a volume notification bubble).
extern const int kTrayPopupSliderHorizontalPadding;

// The minimum/default height of the rows in the system tray menu.
extern const int kTrayPopupItemMinHeight;

// The width used for the first region of the row (which holds an image).
extern const int kTrayPopupItemMinStartWidth;

// The width used for the end region of the row (usually a more arrow).
extern const int kTrayPopupItemMinEndWidth;

// Padding used on right side of labels to keep minimum distance to the next
// item. This applies to all labels in the system menu.
extern const int kTrayPopupLabelRightPadding;

// The width of ToggleButton views including any border padding.
extern const int kTrayToggleButtonWidth;

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
constexpr int kTrayNetworkIconPadding = 2;
constexpr int kUnifiedTrayNetworkIconPadding = 4;

// The size of the icons appearing in the material design system menu.
extern const int kMenuIconSize;
// The size of buttons in the system menu.
ASH_EXPORT extern const int kMenuButtonSize;
// The vertical padding for the system menu separator.
extern const int kMenuSeparatorVerticalPadding;
// The horizontal padding for the system menu separator.
extern const int kMenuExtraMarginFromLeftEdge;
// The visual padding to the left of icons in the system menu.
extern const int kMenuEdgeEffectivePadding;

// The inset applied to clickable surfaces in the system menu that do not have
// the ink drop filling the entire bounds.
extern const int kTrayPopupInkDropInset;

// The radius used to draw the corners of the rounded rect style ink drops.
extern const int kTrayPopupInkDropCornerRadius;

constexpr float kUnifiedMenuBackgroundBlur = 30.f;

// Threshold to ignore update on the slider value.
constexpr float kAudioSliderIgnoreUpdateThreshold = 0.01;

// Duration for the collapse / expand animation in ms.
constexpr int kSystemMenuCollapseExpandAnimationDurationMs = 500;

constexpr gfx::Insets kUnifiedMenuItemPadding(0, 16, 16, 16);
constexpr gfx::Insets kUnifiedSystemInfoViewPadding(0, 16, 16, 16);
constexpr gfx::Insets kUnifiedManagedDeviceViewPadding(0, 16, 11, 16);
constexpr gfx::Insets kUnifiedSliderRowPadding(0, 16, 8, 16);
constexpr gfx::Insets kUnifiedSliderBubblePadding(12, 0, 4, 0);
constexpr gfx::Insets kUnifiedSliderPadding(0, 16);
constexpr gfx::Insets kMicGainSliderViewPadding(0, 52, 0, 0);
constexpr gfx::Insets kMicGainSliderPadding(0, 8, 0, 48);
constexpr int kMicGainSliderViewSpacing = 8;

constexpr int kTrayRadioButtonInterSpacing = 20;
constexpr gfx::Insets kTrayRadioButtonPadding(16, 20, 0, 0);
constexpr gfx::Insets kTraySubLabelPadding(4, 56, 16, 16);

constexpr int kMessageCenterCollapseThreshold = 175;
constexpr int kStackedNotificationBarHeight = 32;
constexpr int kStackedNotificationBarCollapsedHeight = 40;
constexpr int kNotificationIconStackThreshold = 28;
constexpr int kUnifiedSliderViewSpacing = 16;
constexpr int kUnifiedMenuPadding = 8;
constexpr int kUnifiedMessageCenterBubbleSpacing = 8;
constexpr int kUnifiedNotificationCenterSpacing = 16;
constexpr int kUnifiedTrayIconSize = 18;
constexpr int kUnifiedTrayTextTopPadding = 2;
constexpr int kUnifiedTrayTextRightPadding = 1;
constexpr int kUnifiedTrayTimeLeftPadding = 1;
constexpr int kUnifiedTraySpacingBetweenIcons = 6;
constexpr int kUnifiedTrayBatteryWidth = 12;
constexpr int kUnifiedTrayBatteryBottomPadding = 1;
constexpr int kUnifiedTrayCornerRadius = 16;
constexpr int kUnifiedTrayContentPadding = 12;
constexpr int kUnifiedTopShortcutSpacing = 16;
constexpr int kUnifiedNotificationHiddenLineHeight = 20;
constexpr int kUnifiedTopShortcutContainerTopPadding = 12;
constexpr int kUnifiedNotificationMinimumHeight = 40;
constexpr gfx::Insets kUnifiedTopShortcutPadding(0, 16);
constexpr gfx::Insets kUnifiedNotificationHiddenPadding(6, 16);
constexpr gfx::Insets kUnifiedCircularButtonFocusPadding(4);
constexpr gfx::Insets kTrayBackgroundFocusPadding(1);
constexpr gfx::Insets kStackingNotificationClearAllButtonPadding(8, 16);

// Size of an icon drawn inside top shortcut buttons.
// A dark disc with |kTrayItemSize| diameter is drawn in the background.
constexpr int kTrayTopShortcutButtonIconSize = 20;

constexpr int kUnifiedManagedDeviceSpacing = 8;
constexpr int kUnifiedSystemInfoHeight = 16;
constexpr int kUnifiedSystemInfoSpacing = 8;
constexpr gfx::Insets kUnifiedSystemInfoDateViewPadding(3);

// Constants used in StackedNotificationBar located on top of the message
// center.
constexpr gfx::Insets kStackedNotificationIconsContainerPadding(1, 16, 0, 8);
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
constexpr gfx::Size kUnifiedFeaturePodIconSize(46, 46);
constexpr gfx::Size kUnifiedFeaturePodSize(112, 94);
constexpr gfx::Size kUnifiedFeaturePodCollapsedSize(46, 46);
constexpr gfx::Insets kUnifiedFeaturePodIconPadding(5);
constexpr gfx::Insets kUnifiedFeaturePodHoverPadding(2);
constexpr int kUnifiedFeaturePodVectorIconSize = 18;
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
constexpr int kUnifiedNotificationSeparatorThickness = 1;
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

// Constants used in the title row of a detailed view in UnifiedSystemTray.
constexpr gfx::Insets kUnifiedDetailedViewTitlePadding(0, 0, 0, 16);
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
constexpr gfx::Insets kPrivacyScreenToastInsets(10, 16);
constexpr int kPrivacyScreenToastSpacing = 16;

// Constants used for media tray.
constexpr int kMediaTrayPadding = 8;

// There is no active user session during oobe, which means it doesn't support
// dark mode. Sets the icon color to be constant.
constexpr SkColor kIconColorInOobe = gfx::kGoogleGrey700;

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_CONSTANTS_H_
