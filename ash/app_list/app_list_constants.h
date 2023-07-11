// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_CONSTANTS_H_
#define ASH_APP_LIST_APP_LIST_CONSTANTS_H_

#include "ash/app_list/app_list_constants_export.h"
#include "ash/public/cpp/window_properties.h"

namespace ash {

// The key for the property that allows a window to take focus from the app list
// bubble without hiding the bubble.
APP_LIST_CONSTANTS_EXPORT
extern const aura::WindowProperty<bool>* const kAllowGainFocusFromAppListBubble;

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_CONSTANTS_H_
