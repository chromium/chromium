// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PROPERTIES_H_
#define ASH_WM_WINDOW_PROPERTIES_H_

#include "ash/ash_export.h"
#include "ui/base/class_property.h"
#include "ui/base/ui_base_types.h"

namespace aura {
template <typename T>
using WindowProperty = ui::ClassProperty<T>;
}

namespace ash {

class WindowState;

// Shell-specific window property keys; some keys are exported for use in tests.

// Alphabetical sort.

// If this is set to true, the window stays in the same root window even if the
// bounds outside of its root window is set.
ASH_EXPORT extern const aura::WindowProperty<bool>* const kLockedToRootKey;

// Set to true if the window server tells us the window is janky (see
// WindowManagerDelegate::OnWmClientJankinessChanged()).
ASH_EXPORT extern const aura::WindowProperty<bool>* const kWindowIsJanky;

// A property key to store WindowState in the window. The window state
// is owned by the window.
ASH_EXPORT extern const aura::WindowProperty<WindowState*>* const
    kWindowStateKey;

// Alphabetical sort.

}  // namespace ash

#endif  // ASH_WM_WINDOW_PROPERTIES_H_
