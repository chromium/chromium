// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_properties.h"

#include "ash/wm/window_state.h"
#include "ui/base/class_property.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_EXPORT, ash::WindowState*)

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideDuringWindowDragging, false)

DEFINE_UI_CLASS_PROPERTY_KEY(int, kOverrideWindowIconResourceIdKey, -1)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsOverviewItemKey, false)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kLockedToRootKey, false)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kOverviewUiKey, false)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kStayInOverviewOnActivationKey, false)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kWebAuthnRequestId, nullptr)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(WindowState, kWindowStateKey, nullptr)

}  // namespace ash
