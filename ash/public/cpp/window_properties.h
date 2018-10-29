// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_
#define ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_

#include <stdint.h>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/class_property.h"

namespace aura {
class PropertyConverter;
class Window;
template <typename T>
using WindowProperty = ui::ClassProperty<T>;
}

namespace gfx {
class Rect;
}

namespace ash {

namespace mojom {
enum class WindowPinType;
enum class WindowStateType;
}

enum class BackdropWindowMode {
  kEnabled,   // The window needs a backdrop shown behind it.
  kDisabled,  // The window should never have a backdrop.
  kAuto,  // The window manager decides if the window should have a backdrop.
};

// Registers Ash's properties with the given PropertyConverter. This allows Ash
// and other services (eg. Chrome) to exchange Ash window property values.
ASH_PUBLIC_EXPORT void RegisterWindowProperties(
    aura::PropertyConverter* property_converter);

// Shell-specific window property keys for use by ash and its clients.

// Alphabetical sort.

// A property key to store the ARC package name for a window's associated
// ARC app.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<std::string*>* const
    kArcPackageNameKey;

// A property key to specify if the window should (or should not) have a
// backdrop window (typically black) that covers the desktop behind the window.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<BackdropWindowMode>* const
    kBackdropWindowMode;

// If set to true, the window will be replaced by a black rectangle when taking
// screenshot for assistant. Used to preserve privacy for incognito windows.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kBlockedForAssistantSnapshotKey;

// If true, the window can attach into another window.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kCanAttachToAnotherWindowKey;

// If true, will send system keys to the window for dispatch.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kCanConsumeSystemKeysKey;

// A property key to indicate whether we should hide this window in overview
// mode and Alt + Tab.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kHideInOverviewKey;

// Whether the shelf should be hidden when this window is put into fullscreen.
// Exposed because some windows want to explicitly opt-out of this.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kHideShelfWhenFullscreenKey;

// Whether entering fullscreen means that a window should automatically enter
// immersive mode. This is false for some client windows, such as Chrome Apps.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kImmersiveImpliedByFullscreen;

// Whether immersive is currently active (in ImmersiveFullscreenController
// parlance, "enabled").
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kImmersiveIsActive;

// The bounds of the top container in screen coordinates.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<gfx::Rect*>* const
    kImmersiveTopContainerBoundsInScreen;

// The type of window for logging immersive metrics. Type:
// ImmersiveFullscreenController::WindowType.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<int>* const
    kImmersiveWindowType;

// If true, the window is the target window for the tab-dragged window. The key
// is used by overview to show a highlight indication to indicate which overview
// window the dragged tabs will merge into when the user releases the pointer.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kIsDeferredTabDraggingTargetWindowKey;

// If true, the window is a browser window and its tab(s) are currently being
// dragged.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kIsDraggingTabsKey;

// If true, the window is currently showing in overview mode.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kIsShowingInOverviewKey;

// If true (and the window is a panel), it's attached to its shelf item.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kPanelAttachedKey;

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
    mojom::WindowStateType>* const kRestoreWindowStateTypeOverrideKey;

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

// A property key to store the address of the source window that the drag
// originated from if the window is currently in tab-dragging process.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<aura::Window*>* const
    kTabDraggingSourceWindowKey;

// A property key to store the window state type when the tab-dragging finishes.
// This is for Mash to share the window state type before actual state
// transition happens. See TabDragController::IsSnapped, and
// https://crbug.com/880635.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<
    mojom::WindowStateType>* const kTabDroppedWindowStateTypeKey;

// A property key to store the active color on the window frame.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<SkColor>* const
    kFrameActiveColorKey;
// A property key to store the inactive color on the window frame.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<SkColor>* const
    kFrameInactiveColorKey;

// A property key to store ash::WindowPinType for a window.
// When setting this property to PINNED or TRUSTED_PINNED, the window manager
// will try to fullscreen the window and pin it on the top of the screen. If the
// window manager failed to do it, the property will be restored to NONE. When
// setting this property to NONE, the window manager will restore the window.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<mojom::WindowPinType>* const
    kWindowPinTypeKey;

// A property key to indicate whether ash should perform auto management of
// window positions; when you open a second browser, ash will move the two to
// minimize overlap.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kWindowPositionManagedTypeKey;

// A property key to indicate ash's extended window state.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<
    mojom::WindowStateType>* const kWindowStateTypeKey;

// Alphabetical sort.

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_
