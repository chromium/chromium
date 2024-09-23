// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_
#define ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_

#include <stdint.h>
#include <string>

#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/class_property.h"

class SkRegion;

namespace aura {
template <typename T>
using WindowProperty = ui::ClassProperty<T>;
}  // namespace aura

namespace chromeos {
enum class WindowStateType;
}

namespace gfx {
class Rect;
class Size;
}

namespace ash {

class WindowBackdrop;
enum class ResizeShadowType;

// Shell-specific window property keys for use by ash and its clients.

// Alphabetical sort.

// A property key to store the app ID for the window's associated app.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<std::string*>* const
    kAppIDKey;

// A property key to store the ARC Game Controls status.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<ArcGameControlsFlag>* const
    kArcGameControlsFlagsKey;

// A property key to store the ARC package name for a window's associated
// ARC app.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<std::string*>* const
    kArcPackageNameKey;

// If true, the ARC window can not be resized freely.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<ArcResizeLockType>* const
    kArcResizeLockTypeKey;

// A property key to specify whether the window should have backdrop and if
// it has backdrop, the backdrop's mode and type. The backdrop is typically a
// black window that covers the entire workspace placed behind the window.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<WindowBackdrop*>* const
    kWindowBackdropKey;

// If true, will send system keys to the window for dispatch.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kCanConsumeSystemKeysKey;

// Accessibility Id set by the ARC++ client.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<int32_t>* const
    kClientAccessibilityIdKey;

// A property key to exclude the window in MruTracker.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kExcludeInMruKey;

ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kFrameRateThrottleKey;

// A property key to indicate whether we should hide this window in overview
// mode and Alt + Tab.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kHideInOverviewKey;

// A property key to exclude the window in the transient tree iterator.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kExcludeFromTransientTreeTransformKey;

// A property key that ignores window activation changes on the window even if
// it is activatable.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kIgnoreWindowActivationKey;

// A property key to indicate whether we should hide this window in the shelf.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kHideInShelfKey;

// If true, the window is a browser window and its tab(s) are currently being
// dragged.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kIsDraggingTabsKey;

// If true, the window will be ignored when mirroring the desk contents into
// the desk's mini_view.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kHideInDeskMiniViewKey;

// If true, the mirror of the window in the mini_view will be forced to be
// visible and its visibility won't be synced with visibility changes of the
// source.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kForceVisibleInMiniViewKey;

// Set on lacros browser windows and identifies the lacros profile used to
// launch the browser. See desk_profiles_delegate.h for more information.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<uint64_t>* const
    kLacrosProfileId;

// A property key to store whether we should minimize a window when a system
// synthesized back event (back gesture, back button) is processed by this
// window and when this window is at the bottom of its navigation stack.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool*>* const
    kMinimizeOnBackKey;

// If true, the current PIP window is spawned from this window.
// Android PIP has two types of behavior depending on how many activities the
// original task has before entering PIP.
// SAPIP(Single-activity PIP): If the original task has only one activity, PIP
// can be handled as window state change of the target window. In this case, the
// PIP original window is this exact PIP window.
// MAPIP(Multi-activity PIP): If the original task has more than one activities,
// a new window is created for PIP, which is a completely different one from
// the existing window. This existing window is the original window of the
// current PIP window in this case. This property is used, for example, to
// calculated the position of the PIP window in the Alt-Tab window cycler.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kPipOriginalWindowKey;

// A property key to store the PIP snap fraction for this window.
// The fraction is defined in a clockwise fashion against the PIP movement area.
//
//            0   1
//          4 +---+ 1
//            |   |
//          3 +---+ 2
//            3   2
//
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<float*>* const
    kPipSnapFractionKey;

// A property key which stores the preferred size used when the unresizable
// window is snapped in clamshell mode. Setting this property can make the
// window snappable even if it's unresizable. Please note that the window
// doesn't become snappable if the width (height if in the portrait snap mode)
// of the property value is bigger than one of the workspace or is equal to 0.
// Also, setting the zero size (width=0 and height=0) causes DCHECK failure.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<gfx::Size*>* const
    kUnresizableSnappedSizeKey;

// Maps to ws::mojom::WindowManager::kRenderParentTitleArea_Property.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kRenderTitleAreaProperty;

// A property key which stores the bounds in screen coordinates to restore a
// window to. These take preference over the current bounds. This is used by
// e.g. the tablet mode window manager.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<gfx::Rect*>* const
    kRestoreBoundsOverrideKey;

// A property key which stores the window state to restore a window to. These
// take preference over the current state if
// |kRestoreWindowStateTypeOverrideKey| is set. This is used by e.g. the tablet
// mode window manager.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<
    chromeos::WindowStateType>* const kRestoreWindowStateTypeOverrideKey;

// A property key to store whether search key accelerator is reserved for a
// window. This is used to pass through search key accelerators to Android
// window if user is navigating with TalkBack (screen reader on Android).
// TalkBack uses search key as a modifier key.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kSearchKeyAcceleratorReservedKey;

// A property key to store the serialized id for a window's shelf item.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<std::string*>* const
    kShelfIDKey;

// A property key to store the type of a window's shelf item.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<int32_t>* const
    kShelfItemTypeKey;

// A property key to store the type of a window's resize shadow.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<ResizeShadowType>* const
    kResizeShadowTypeKey;

// A property key to store the system gesture exclusion region. From a point
// inside the region, system gesture e.g. back gesture shouldn't be triggered.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<SkRegion*>* const
    kSystemGestureExclusionKey;

// A property key to indicate whether ash should perform auto management of
// window positions; when you open a second browser, ash will move the two to
// minimize overlap.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kWindowPositionManagedTypeKey;

// A property key to indicate whether the cursor should stay visible when a key
// is pressed. ChromeOS normally hides the cursor when a key is pressed but this
// results in undesirable interaction with games.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kShowCursorOnKeypress;

// A property key to indicate pip window state.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kWindowPipTypeKey;

// A property key which store the bounds of a pip window's resize handle in the
// pip window's coordinates. This is used to track the usage of the resize
// handle.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<gfx::Rect*>* const
    kWindowPipResizeHandleBoundsKey;

// Alphabetical sort.

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_
