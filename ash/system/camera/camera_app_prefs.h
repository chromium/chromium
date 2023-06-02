// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAMERA_CAMERA_APP_PREFS_H_
#define ASH_SYSTEM_CAMERA_CAMERA_APP_PREFS_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;

namespace ash::camera_app_prefs {

ASH_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

ASH_EXPORT bool ShouldDevToolsOpen();

ASH_EXPORT void SetDevToolsOpenState(bool is_opened);

}  // namespace ash::camera_app_prefs

#endif  // ASH_SYSTEM_CAMERA_CAMERA_APP_PREFS_H_
