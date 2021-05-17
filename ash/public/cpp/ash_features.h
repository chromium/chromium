// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_FEATURES_H_
#define ASH_PUBLIC_CPP_ASH_FEATURES_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/feature_list.h"

namespace ash {
namespace features {

// Enables the UI to support Ambient EQ if the device supports it.
// See https://crbug.com/1021193 for more details.
ASH_PUBLIC_EXPORT extern const base::Feature kAllowAmbientEQ;

// Enables the pre-load app window for ARC++ app during ARCVM booting stage on
// full restore process.
ASH_PUBLIC_EXPORT extern const base::Feature kArcGhostWindow;

// Enables resize lock for ARC++ and puts restrictions on window resizing.
// TODO(takise): Remove this after the feature is fully launched.
ASH_PUBLIC_EXPORT extern const base::Feature kArcResizeLock;

// Enables the Auto Night Light feature which sets the default schedule type to
// sunset-to-sunrise until the user changes it to something else. This feature
// is not exposed to the end user, and is enabled only via cros_config for
// certain devices.
ASH_PUBLIC_EXPORT extern const base::Feature kAutoNightLight;

// Enables the Capture Mode feature which is an enhanced screenshot and screen
// capture user experience.
ASH_PUBLIC_EXPORT extern const base::Feature kCaptureMode;

// Enables compositing-based throttling to throttle appropriate frame sinks that
// do not need to be refreshed at high fps.
ASH_PUBLIC_EXPORT extern const base::Feature kCompositingBasedThrottling;

// Enables contextual nudges for gesture education.
ASH_PUBLIC_EXPORT extern const base::Feature kContextualNudges;

// Enables dark/light mode feature.
ASH_PUBLIC_EXPORT extern const base::Feature kDarkLightMode;

// Enables indicators to hint where displays are connected.
ASH_PUBLIC_EXPORT extern const base::Feature kDisplayAlignAssist;

// Enables identification overlays on each display.
ASH_PUBLIC_EXPORT extern const base::Feature kDisplayIdentification;

// Enables the docked (a.k.a. picture-in-picture) magnifier.
// TODO(afakhry): Remove this after the feature is fully launched.
// https://crbug.com/709824.
ASH_PUBLIC_EXPORT extern const base::Feature kDockedMagnifier;

// Enables the full restore feature. If this is enabled, we will restore apps
// and app windows after a crash or reboot.
ASH_PUBLIC_EXPORT extern const base::Feature kFullRestore;

// Limits the windows listed in Alt-Tab to the ones in the currently active
// desk.
ASH_PUBLIC_EXPORT extern const base::Feature kLimitAltTabToActiveDesk;

// Enables notifications on the lock screen.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenNotifications;

// Enables inline reply on notifications on the lock screen.
// This option is effective when |kLockScreenNotification| is enabled.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenInlineReply;

// Supports the feature to hide sensitive content in notifications on the lock
// screen. This option is effective when |kLockScreenNotification| is enabled.
ASH_PUBLIC_EXPORT extern const base::Feature
    kLockScreenHideSensitiveNotificationsSupport;

// Enables lock screen media controls UI and use of media keys on the lock
// screen.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenMediaControls;

// Enables hiding of ARC media notifications. If this is enabled, all ARC
// notifications that are of the media type will not be shown. This
// is because they will be replaced by native media session notifications.
// TODO(beccahughes): Remove after launch. (https://crbug.com/897836)
ASH_PUBLIC_EXPORT extern const base::Feature kHideArcMediaNotifications;

// Enables using arrow keys for display arrangement in display settings page.
ASH_PUBLIC_EXPORT extern const base::Feature
    kKeyboardBasedDisplayArrangementInSettings;

// Enables the redesigned managed device info UI in the system tray.
ASH_PUBLIC_EXPORT extern const base::Feature kManagedDeviceUIRedesign;

// Enables resizing/moving the selection region for partial screenshot.
ASH_PUBLIC_EXPORT extern const base::Feature kMovablePartialScreenshot;

// Enables the Night Light feature.
ASH_PUBLIC_EXPORT extern const base::Feature kNightLight;

// Enabled notification expansion animation.
ASH_PUBLIC_EXPORT extern const base::Feature kNotificationExpansionAnimation;

// Shorten notification timeouts to 6 seconds.
ASH_PUBLIC_EXPORT extern const base::Feature
    kNotificationExperimentalShortTimeouts;

// Enables notification scroll bar in UnifiedSystemTray.
ASH_PUBLIC_EXPORT extern const base::Feature kNotificationScrollBar;

// Limits the items on the shelf to the ones associated with windows the
// currently active desk.
ASH_PUBLIC_EXPORT extern const base::Feature kPerDeskShelf;

// Enables rounded corners for the Picture-in-picture window.
ASH_PUBLIC_EXPORT extern const base::Feature kPipRoundedCorners;

// Enables suppression of Displays notifications other than resolution change.
ASH_PUBLIC_EXPORT extern const base::Feature kReduceDisplayNotifications;

// Enables displaying separate network icons for different networks types.
// https://crbug.com/902409
ASH_PUBLIC_EXPORT extern const base::Feature kSeparateNetworkIcons;

// Enables trilinear filtering.
ASH_PUBLIC_EXPORT extern const base::Feature kTrilinearFiltering;

// Enables using the BluetoothSystem Mojo interface for Bluetooth operations.
ASH_PUBLIC_EXPORT extern const base::Feature kUseBluetoothSystemInAsh;

// Enables background blur for the app list, shelf, unified system tray,
// autoclick menu, etc. Also enables the AppsGridView mask layer, slower devices
// may have choppier app list animations while in this mode. crbug.com/765292.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableBackgroundBlur;

// When enabled, shelf navigation controls and the overview tray item will be
// removed from the shelf in tablet mode (unless otherwise specified by user
// preferences, or policy).
ASH_PUBLIC_EXPORT extern const base::Feature kHideShelfControlsInTabletMode;

// When enabled, the overivew and desk reverse scrolling behaviors are changed
// and if the user performs the old gestures, a notification or toast will show
// up.
// TODO(https://crbug.com/1107183): Remove this after the feature is launched.
ASH_PUBLIC_EXPORT extern const base::Feature kReverseScrollGestures;

// When enabled, there will be an alert bubble showing up when the device
// returns from low brightness (e.g., sleep, closed cover) without a lock screen
// and the active window is in fullscreen.
// TODO(https://crbug.com/1107185): Remove this after the feature is launched.
ASH_PUBLIC_EXPORT extern const base::Feature kFullscreenAlertBubble;

// Enables battery indicator for styluses in the palette tray
ASH_PUBLIC_EXPORT extern const base::Feature kStylusBatteryStatus;

// Enables vertical split screen for clamshell mode. This allows users to snap
// top and bottom when the screen is in portrait orientation, while snap left
// and right when the screen is in landscape orientation.
ASH_PUBLIC_EXPORT extern const base::Feature kVerticalSplitScreen;

// Enables special handling of Chrome tab drags from a WebUI tab strip.
// These will be treated similarly to a window drag, showing split view
// indicators in tablet mode, etc. The functionality is behind a flag
// right now since it is under development.
ASH_PUBLIC_EXPORT extern const base::Feature kWebUITabStripTabDragIntegration;

// Change window creation to be based on cursor position
// when there are multiple displays.
ASH_PUBLIC_EXPORT extern const base::Feature kWindowsFollowCursor;

// Enables notifications to be shown within context menus.
ASH_PUBLIC_EXPORT extern const base::Feature kNotificationsInContextMenu;

// Enables ARC integration with the productivity feature that aims to reduce
// context switching by enabling users to collect content and transfer or access
// it later.
ASH_PUBLIC_EXPORT extern const base::Feature kHoldingSpaceArcIntegration;

// Enables dragging an unpinned open app to pinned app side to pin.
ASH_PUBLIC_EXPORT extern const base::Feature kDragUnpinnedAppToPin;

// Enables the system tray to show more information in larger screen.
ASH_PUBLIC_EXPORT extern const base::Feature kScalableStatusArea;

// Enables the system tray to show date in sufficiently large screen.
ASH_PUBLIC_EXPORT extern const base::Feature kShowDateInTrayButton;

ASH_PUBLIC_EXPORT bool IsAllowAmbientEQEnabled();

ASH_PUBLIC_EXPORT bool IsAltTabLimitedToActiveDesk();

ASH_PUBLIC_EXPORT bool IsPerDeskShelfEnabled();

ASH_PUBLIC_EXPORT bool IsArcGhostWindowEnabled();

ASH_PUBLIC_EXPORT bool IsArcResizeLockEnabled();

ASH_PUBLIC_EXPORT bool IsAutoNightLightEnabled();

ASH_PUBLIC_EXPORT bool IsCaptureModeEnabled();

ASH_PUBLIC_EXPORT bool IsCompositingBasedThrottlingEnabled();

ASH_PUBLIC_EXPORT bool IsDarkLightModeEnabled();

ASH_PUBLIC_EXPORT bool IsFullRestoreEnabled();

ASH_PUBLIC_EXPORT bool IsHideArcMediaNotificationsEnabled();

ASH_PUBLIC_EXPORT bool IsKeyboardBasedDisplayArrangementInSettingsEnabled();

ASH_PUBLIC_EXPORT bool IsKeyboardShortcutViewerAppEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenNotificationsEnabled();

ASH_PUBLIC_EXPORT bool IsManagedDeviceUIRedesignEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenInlineReplyEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenHideSensitiveNotificationsSupported();

ASH_PUBLIC_EXPORT bool IsNotificationExpansionAnimationEnabled();

ASH_PUBLIC_EXPORT bool IsNotificationExperimentalShortTimeoutsEnabled();

ASH_PUBLIC_EXPORT bool IsNotificationScrollBarEnabled();

ASH_PUBLIC_EXPORT bool IsPipRoundedCornersEnabled();

ASH_PUBLIC_EXPORT bool IsSeparateNetworkIconsEnabled();

ASH_PUBLIC_EXPORT bool IsTrilinearFilteringEnabled();

ASH_PUBLIC_EXPORT bool IsBackgroundBlurEnabled();

ASH_PUBLIC_EXPORT bool IsReduceDisplayNotificationsEnabled();

ASH_PUBLIC_EXPORT bool IsHideShelfControlsInTabletModeEnabled();

ASH_PUBLIC_EXPORT bool IsReverseScrollGesturesEnabled();

ASH_PUBLIC_EXPORT bool IsFullscreenAlertBubbleEnabled();

ASH_PUBLIC_EXPORT bool AreContextualNudgesEnabled();

ASH_PUBLIC_EXPORT bool IsStylusBatteryStatusEnabled();

ASH_PUBLIC_EXPORT bool IsDisplayIdentificationEnabled();

ASH_PUBLIC_EXPORT bool IsVerticalSplitScreenEnabled();

ASH_PUBLIC_EXPORT bool IsWebUITabStripTabDragIntegrationEnabled();

ASH_PUBLIC_EXPORT bool IsDisplayAlignmentAssistanceEnabled();

ASH_PUBLIC_EXPORT bool IsNotificationsInContextMenuEnabled();

ASH_PUBLIC_EXPORT bool IsHoldingSpaceArcIntegrationEnabled();

ASH_PUBLIC_EXPORT bool IsDragUnpinnedAppToPinEnabled();

ASH_PUBLIC_EXPORT bool IsScalableStatusAreaEnabled();

ASH_PUBLIC_EXPORT bool IsShowDateInTrayButtonEnabled();

// These two functions are supposed to be temporary functions to set or get
// whether "WebUITabStrip" feature is enabled from Chrome.
ASH_PUBLIC_EXPORT void SetWebUITabStripEnabled(bool enabled);
ASH_PUBLIC_EXPORT bool IsWebUITabStripEnabled();

ASH_PUBLIC_EXPORT bool DoWindowsFollowCursor();

}  // namespace features
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_FEATURES_H_
