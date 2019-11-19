// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_SWITCHES_H_
#define ASH_PUBLIC_CPP_ASH_SWITCHES_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {
namespace switches {

// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see chromeos::LoginUtil::GetOffTheRecordCommandLine().)

// Please keep alphabetized.
// TODO(sky): fix order!
ASH_PUBLIC_EXPORT extern const char kAshColorMode[];
ASH_PUBLIC_EXPORT extern const char kAshColorModeDark[];
ASH_PUBLIC_EXPORT extern const char kAshColorModeLight[];
ASH_PUBLIC_EXPORT extern const char kAshConstrainPointerToRoot[];
ASH_PUBLIC_EXPORT extern const char kAshDebugShortcuts[];
ASH_PUBLIC_EXPORT extern const char kAshDeveloperShortcuts[];
ASH_PUBLIC_EXPORT extern const char kAshDisableTouchExplorationMode[];
ASH_PUBLIC_EXPORT extern const char kAshEnableCursorMotionBlur[];
ASH_PUBLIC_EXPORT extern const char kAshEnableV1AppBackButton[];
ASH_PUBLIC_EXPORT extern const char kAshEnableMagnifierKeyScroller[];
ASH_PUBLIC_EXPORT extern const char kAshEnablePaletteOnAllDisplays[];
ASH_PUBLIC_EXPORT extern const char kAshEnableTabletMode[];
ASH_PUBLIC_EXPORT extern const char kAshEnableWaylandServer[];
ASH_PUBLIC_EXPORT extern const char kAshForceEnableStylusTools[];
ASH_PUBLIC_EXPORT extern const char kAshForceStatusAreaCollapsible[];
ASH_PUBLIC_EXPORT extern const char kAshPowerButtonPosition[];
ASH_PUBLIC_EXPORT extern const char kAshUiMode[];
ASH_PUBLIC_EXPORT extern const char kAshUiModeClamshell[];
ASH_PUBLIC_EXPORT extern const char kAshUiModeTablet[];
ASH_PUBLIC_EXPORT extern const char kAshHideNotificationsForFactory[];
ASH_PUBLIC_EXPORT extern const char kAshShelfColor[];
ASH_PUBLIC_EXPORT extern const char kAshShelfColorEnabled[];
ASH_PUBLIC_EXPORT extern const char kAshShelfColorDisabled[];
ASH_PUBLIC_EXPORT extern const char kAshShelfColorScheme[];
ASH_PUBLIC_EXPORT extern const char kAshShelfColorSchemeLightMuted[];
ASH_PUBLIC_EXPORT extern const char kAshShelfColorSchemeLightVibrant[];
ASH_PUBLIC_EXPORT extern const char kAshShelfColorSchemeNormalMuted[];
ASH_PUBLIC_EXPORT extern const char kAshShelfColorSchemeNormalVibrant[];
ASH_PUBLIC_EXPORT extern const char kAshShelfColorSchemeDarkMuted[];
ASH_PUBLIC_EXPORT extern const char kAshShelfColorSchemeDarkVibrant[];
ASH_PUBLIC_EXPORT extern const char kAshSideVolumeButtonPosition[];
ASH_PUBLIC_EXPORT extern const char kAshTouchHud[];
ASH_PUBLIC_EXPORT extern const char kAuraLegacyPowerButton[];
ASH_PUBLIC_EXPORT extern const char kEnableDimShelf[];
ASH_PUBLIC_EXPORT extern const char kForceTabletPowerButton[];
ASH_PUBLIC_EXPORT extern const char kHasInternalStylus[];
ASH_PUBLIC_EXPORT extern const char kShowTaps[];
ASH_PUBLIC_EXPORT extern const char kShowWebUiLock[];
ASH_PUBLIC_EXPORT extern const char kShowWebUiLogin[];
ASH_PUBLIC_EXPORT extern const char kSuppressMessageCenterPopups[];
ASH_PUBLIC_EXPORT extern const char kTouchscreenUsableWhileScreenOff[];

ASH_PUBLIC_EXPORT bool IsUsingViewsLock();
ASH_PUBLIC_EXPORT bool IsUsingShelfAutoDim();

}  // namespace switches
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_SWITCHES_H_
