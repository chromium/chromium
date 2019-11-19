// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"

#include "base/command_line.h"

namespace ash {
namespace switches {

// Indicates the current color mode of ash.
const char kAshColorMode[] = "ash-color-mode";
const char kAshColorModeDark[] = "dark";
const char kAshColorModeLight[] = "light";

// Force the pointer (cursor) position to be kept inside root windows.
const char kAshConstrainPointerToRoot[] = "ash-constrain-pointer-to-root";

// Enable keyboard shortcuts useful for debugging.
const char kAshDebugShortcuts[] = "ash-debug-shortcuts";

// Enable keyboard shortcuts used by developers only.
const char kAshDeveloperShortcuts[] = "ash-dev-shortcuts";

// Disable the Touch Exploration Mode. Touch Exploration Mode will no longer be
// turned on automatically when spoken feedback is enabled when this flag is
// set.
const char kAshDisableTouchExplorationMode[] =
    "ash-disable-touch-exploration-mode";

// Enables Backbutton on frame for v1 apps.
// TODO(oshima): Remove this once the feature is launched. crbug.com/749713.
const char kAshEnableV1AppBackButton[] = "ash-enable-v1-app-back-button";

// Enable cursor motion blur.
const char kAshEnableCursorMotionBlur[] = "ash-enable-cursor-motion-blur";

// Enables key bindings to scroll magnified screen.
const char kAshEnableMagnifierKeyScroller[] =
    "ash-enable-magnifier-key-scroller";

// Enables the palette on every display, instead of only the internal one.
const char kAshEnablePaletteOnAllDisplays[] =
    "ash-enable-palette-on-all-displays";

// If the flag is present, it indicates 1) the device has accelerometer and 2)
// the device is a convertible device or a tablet device (thus is capable of
// entering tablet mode). If this flag is not set, then the device is not
// capable of entering tablet mode. For example, Samus has accelerometer, but
// is not a covertible or tablet, thus doesn't have this flag set, thus can't
// enter tablet mode.
const char kAshEnableTabletMode[] = "enable-touchview";

// Enable the wayland server.
const char kAshEnableWaylandServer[] = "enable-wayland-server";

// Enables the stylus tools next to the status area.
const char kAshForceEnableStylusTools[] = "force-enable-stylus-tools";

// Forces the status area to allow collapse/expand regardless of the current
// state.
const char kAshForceStatusAreaCollapsible[] = "force-status-area-collapsible";

// Power button position includes the power button's physical display side and
// the percentage for power button center position to the display's
// width/height in landscape_primary screen orientation. The value is a JSON
// object containing a "position" property with the value "left", "right",
// "top", or "bottom". For "left" and "right", a "y" property specifies the
// button's center position as a fraction of the display's height (in [0.0,
// 1.0]) relative to the top of the display. For "top" and "bottom", an "x"
// property gives the position as a fraction of the display's width relative to
// the left side of the display.
const char kAshPowerButtonPosition[] = "ash-power-button-position";

// Enables required things for the selected UI mode, regardless of whether the
// Chromebook is currently in the selected UI mode.
const char kAshUiMode[] = "force-tablet-mode";

// Values for the kAshUiMode flag.
const char kAshUiModeClamshell[] = "clamshell";
const char kAshUiModeTablet[] = "touch_view";

// Hides notifications that are irrelevant to Chrome OS device factory testing,
// such as battery level updates.
const char kAshHideNotificationsForFactory[] =
    "ash-hide-notifications-for-factory";

// Enables the heads-up display for tracking touch points.
const char kAshTouchHud[] = "ash-touch-hud";

// The physical position info of the side volume button while in landscape
// primary screen orientation. The value is a JSON object containing a "region"
// property with the value "keyboard", "screen" and a "side" property with the
// value "left", "right", "top", "bottom".
const char kAshSideVolumeButtonPosition[] = "ash-side-volume-button-position";

// (Most) Chrome OS hardware reports ACPI power button releases correctly.
// Standard hardware reports releases immediately after presses.  If set, we
// lock the screen or shutdown the system immediately in response to a press
// instead of displaying an interactive animation.
const char kAuraLegacyPowerButton[] = "aura-legacy-power-button";

// Enables Shelf Dimming for ChromeOS.
const char kEnableDimShelf[] = "enable-dim-shelf";

// If set, tablet-like power button behavior (i.e. tapping the button turns the
// screen off) is used even if the device is in laptop mode.
const char kForceTabletPowerButton[] = "force-tablet-power-button";

// Whether this device has an internal stylus.
const char kHasInternalStylus[] = "has-internal-stylus";

// Draws a circle at each touch point, similar to the Android OS developer
// option "Show taps".
const char kShowTaps[] = "show-taps";

// If true, the webui lock screen wil be shown. This is deprecated and will be
// removed in the future.
const char kShowWebUiLock[] = "show-webui-lock";

// Forces the webui login implementation.
const char kShowWebUiLogin[] = "show-webui-login";

// Chromebases' touchscreens can be used to wake from suspend, unlike the
// touchscreens on other Chrome OS devices. If set, the touchscreen is kept
// enabled while the screen is off so that it can be used to turn the screen
// back on after it has been turned off for inactivity but before the system has
// suspended.
const char kTouchscreenUsableWhileScreenOff[] =
    "touchscreen-usable-while-screen-off";

// Hides all Message Center notification popups (toasts). Used for testing.
const char kSuppressMessageCenterPopups[] = "suppress-message-center-popups";

bool IsUsingViewsLock() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(kShowWebUiLock);
}

bool IsUsingShelfAutoDim() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kEnableDimShelf);
}

}  // namespace switches
}  // namespace ash
