// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_UTILS_H_
#define ASH_SYSTEM_TRAY_TRAY_UTILS_H_

#include "components/session_manager/session_manager_types.h"
#include "third_party/skia/include/core/SkColor.h"

namespace views {
class Label;
}

namespace ash {

// Sets up a Label properly for the tray (sets color, font etc.).
void SetupLabelForTray(views::Label* label);

// Gets the current tray icon color for the given session state.
SkColor TrayIconColor(session_manager::SessionState session_state);

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_UTILS_H_
