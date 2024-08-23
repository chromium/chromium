// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PROPERTIES_H_
#define ASH_WM_WINDOW_PROPERTIES_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/base/class_property.h"

namespace aura {
template <typename T>
using WindowProperty = ui::ClassProperty<T>;
}

namespace ash {

class WindowState;

// Shell-specific window property keys; some keys are exported for use in tests.

// Alphabetical sort.

// A property key to indicate whether this window is temporarily hidden because
// of the window dragging.
ASH_EXPORT extern const aura::WindowProperty<bool>* const
    kHideDuringWindowDragging;

// Used to override the icon image used for window's icon.
ASH_EXPORT extern const aura::WindowProperty<int>* const
    kOverrideWindowIconResourceIdKey;

// A property key attached to the overview windows (contains header, backdrop,
// etc.). Overview windows behave a little different from windows tagged with
// just `kOverviewUiKey`. For instance, they do not get moved if the active desk
// is destroyed.
extern const aura::WindowProperty<bool>* const kIsOverviewItemKey;

// If this is set to true, the window stays in the same root window even if the
// bounds outside of its root window is set.
ASH_EXPORT extern const aura::WindowProperty<bool>* const kLockedToRootKey;

// A property key indicating that this window is created as part of the overview
// mode UI. These windows will not cause overview to end on activation, will not
// show up in the MRU tracker and will be moved to the active desk on desk
// change.
ASH_EXPORT extern const aura::WindowProperty<bool>* const kOverviewUiKey;

// If we are in overview mode, and then activate a window, we will normally exit
// overview. Set this property to true if that behavior is not desired.
ASH_EXPORT extern const aura::WindowProperty<bool>* const
    kStayInOverviewOnActivationKey;

// A property key indicating a unique WebAuthn request id for ash to locate the
// window initiating the request.
ASH_EXPORT extern const aura::WindowProperty<std::string*>* const
    kWebAuthnRequestId;

// A property key to store WindowState in the window. The window state
// is owned by the window.
ASH_EXPORT extern const aura::WindowProperty<WindowState*>* const
    kWindowStateKey;

// Alphabetical sort.

}  // namespace ash

#endif  // ASH_WM_WINDOW_PROPERTIES_H_
