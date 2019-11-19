// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_properties.h"

#include "ash/wm/window_state.h"
#include "ui/gfx/geometry/rect.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_EXPORT, ash::WindowState*)

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kLockedToRootKey, false)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowIsJanky, false)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ash::WindowState, kWindowStateKey, nullptr)

}  // namespace ash
