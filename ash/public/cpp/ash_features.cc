// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"

#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace ash {
namespace features {

const base::Feature kArcGhostWindow{"ArcGhostWindow",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAllowAmbientEQ{"AllowAmbientEQ",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kArcResizeLock{"ArcResizeLock",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutoNightLight{"AutoNightLight",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCaptureMode{"CaptureMode",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCompositingBasedThrottling{
    "CompositingBasedThrottling", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualNudges{"ContextualNudges",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDarkLightMode{"DarkLightMode",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDisplayAlignAssist{"DisplayAlignAssist",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDisplayIdentification{"DisplayIdentification",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDockedMagnifier{"DockedMagnifier",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kFullRestore{"FullRestore",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLimitAltTabToActiveDesk{"LimitAltTabToActiveDesk",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLockScreenNotifications{"LockScreenNotifications",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLockScreenInlineReply{"LockScreenInlineReply",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLockScreenHideSensitiveNotificationsSupport{
    "LockScreenHideSensitiveNotificationsSupport",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLockScreenMediaControls{"LockScreenMediaControls",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHideArcMediaNotifications{
    "HideArcMediaNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kManagedDeviceUIRedesign{"ManagedDeviceUIRedesign",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNightLight{"NightLight", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNotificationExpansionAnimation{
    "NotificationExpansionAnimation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNotificationExperimentalShortTimeouts{
    "NotificationExperimentalShortTimeouts", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNotificationScrollBar{"NotificationScrollBar",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPerDeskShelf{"PerDeskShelf",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPipRoundedCorners{"PipRoundedCorners",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReduceDisplayNotifications{
    "ReduceDisplayNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSeparateNetworkIcons{"SeparateNetworkIcons",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTrilinearFiltering{"TrilinearFiltering",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseBluetoothSystemInAsh{"UseBluetoothSystemInAsh",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableBackgroundBlur{"EnableBackgroundBlur",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHideShelfControlsInTabletMode{
    "HideShelfControlsInTabletMode", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kReverseScrollGestures{"EnableReverseScrollGestures",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kFullscreenAlertBubble{"EnableFullscreenBubble",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kStylusBatteryStatus{"StylusBatteryStatus",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kVerticalSplitScreen{"VerticalSplitScreen",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebUITabStripTabDragIntegration{
    "WebUITabStripTabDragIntegration", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWindowsFollowCursor{"WindowsFollowCursor",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNotificationsInContextMenu{
    "NotificationsInContextMenu", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHoldingSpaceArcIntegration{
    "HoldingSpaceArcIntegration", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDragUnpinnedAppToPin{"DragUnpinnedAppToPin",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kScalableStatusArea{"ScalableStatusArea",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kShowDateInTrayButton{"ShowDateInTrayButton",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kKeyboardBasedDisplayArrangementInSettings{
    "KeyboardBasedDisplayArrangementInSettings",
    base::FEATURE_ENABLED_BY_DEFAULT};

bool IsArcGhostWindowEnabled() {
  return base::FeatureList::IsEnabled(kFullRestore) &&
         base::FeatureList::IsEnabled(kArcGhostWindow);
}

bool IsAllowAmbientEQEnabled() {
  return base::FeatureList::IsEnabled(kAllowAmbientEQ);
}

bool IsAltTabLimitedToActiveDesk() {
  return base::FeatureList::IsEnabled(kLimitAltTabToActiveDesk);
}

bool IsArcResizeLockEnabled() {
  return base::FeatureList::IsEnabled(kArcResizeLock);
}

bool IsPerDeskShelfEnabled() {
  return base::FeatureList::IsEnabled(kPerDeskShelf);
}

bool IsAutoNightLightEnabled() {
  return base::FeatureList::IsEnabled(kAutoNightLight);
}

bool IsCaptureModeEnabled() {
  return base::FeatureList::IsEnabled(kCaptureMode);
}

bool IsCompositingBasedThrottlingEnabled() {
  return base::FeatureList::IsEnabled(kCompositingBasedThrottling);
}

bool IsDarkLightModeEnabled() {
  return base::FeatureList::IsEnabled(kDarkLightMode);
}

bool IsFullRestoreEnabled() {
  return base::FeatureList::IsEnabled(kFullRestore);
}

bool IsHideArcMediaNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kHideArcMediaNotifications);
}

bool IsKeyboardBasedDisplayArrangementInSettingsEnabled() {
  return base::FeatureList::IsEnabled(
      kKeyboardBasedDisplayArrangementInSettings);
}

bool IsLockScreenNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenNotifications);
}

bool IsLockScreenInlineReplyEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenInlineReply);
}

bool IsLockScreenHideSensitiveNotificationsSupported() {
  return base::FeatureList::IsEnabled(
      kLockScreenHideSensitiveNotificationsSupport);
}

bool IsManagedDeviceUIRedesignEnabled() {
  return base::FeatureList::IsEnabled(kManagedDeviceUIRedesign);
}

bool IsNotificationExpansionAnimationEnabled() {
  return base::FeatureList::IsEnabled(kNotificationExpansionAnimation);
}

bool IsNotificationScrollBarEnabled() {
  return base::FeatureList::IsEnabled(kNotificationScrollBar);
}

bool IsNotificationExperimentalShortTimeoutsEnabled() {
  return base::FeatureList::IsEnabled(kNotificationExperimentalShortTimeouts);
}

bool IsPipRoundedCornersEnabled() {
  return base::FeatureList::IsEnabled(kPipRoundedCorners);
}

bool IsSeparateNetworkIconsEnabled() {
  return base::FeatureList::IsEnabled(kSeparateNetworkIcons);
}

bool IsTrilinearFilteringEnabled() {
  static bool use_trilinear_filtering =
      base::FeatureList::IsEnabled(kTrilinearFiltering);
  return use_trilinear_filtering;
}

bool IsBackgroundBlurEnabled() {
  bool enabled_by_feature_flag =
      base::FeatureList::IsEnabled(kEnableBackgroundBlur);
#if defined(ARCH_CPU_ARM_FAMILY)
  // Enable background blur on Mali when GPU rasterization is enabled.
  // See crbug.com/996858 for the condition.
  return enabled_by_feature_flag &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             ash::switches::kAshEnableTabletMode);
#else
  return enabled_by_feature_flag;
#endif
}

bool IsReduceDisplayNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kReduceDisplayNotifications);
}

bool IsHideShelfControlsInTabletModeEnabled() {
  return base::FeatureList::IsEnabled(kHideShelfControlsInTabletMode);
}

bool IsReverseScrollGesturesEnabled() {
  return base::FeatureList::IsEnabled(kReverseScrollGestures);
}

bool IsFullscreenAlertBubbleEnabled() {
  return base::FeatureList::IsEnabled(kFullscreenAlertBubble);
}

bool AreContextualNudgesEnabled() {
  if (!IsHideShelfControlsInTabletModeEnabled())
    return false;
  return base::FeatureList::IsEnabled(kContextualNudges);
}

bool IsStylusBatteryStatusEnabled() {
  return base::FeatureList::IsEnabled(kStylusBatteryStatus);
}

bool IsDisplayIdentificationEnabled() {
  return base::FeatureList::IsEnabled(kDisplayIdentification);
}

bool IsVerticalSplitScreenEnabled() {
  return base::FeatureList::IsEnabled(kVerticalSplitScreen);
}

bool IsWebUITabStripTabDragIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kWebUITabStripTabDragIntegration);
}

bool IsDisplayAlignmentAssistanceEnabled() {
  return base::FeatureList::IsEnabled(kDisplayAlignAssist);
}

bool IsNotificationsInContextMenuEnabled() {
  return base::FeatureList::IsEnabled(kNotificationsInContextMenu);
}

bool IsHoldingSpaceArcIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kHoldingSpaceArcIntegration);
}

bool IsDragUnpinnedAppToPinEnabled() {
  return base::FeatureList::IsEnabled(kDragUnpinnedAppToPin);
}

bool IsScalableStatusAreaEnabled() {
  return base::FeatureList::IsEnabled(kScalableStatusArea);
}

bool IsShowDateInTrayButtonEnabled() {
  return IsScalableStatusAreaEnabled() &&
         base::FeatureList::IsEnabled(kShowDateInTrayButton);
}

bool DoWindowsFollowCursor() {
  return base::FeatureList::IsEnabled(kWindowsFollowCursor);
}

namespace {

// The boolean flag indicating if "WebUITabStrip" feature is enabled in Chrome.
bool g_webui_tab_strip_enabled = false;

}  // namespace

void SetWebUITabStripEnabled(bool enabled) {
  g_webui_tab_strip_enabled = enabled;
}

bool IsWebUITabStripEnabled() {
  return g_webui_tab_strip_enabled;
}

}  // namespace features
}  // namespace ash
