// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_SWITCHES_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_SWITCHES_H_

#include "ash/public/cpp/ash_public_export.h"
#include "build/build_config.h"

namespace ash {
namespace switches {

// Please keep these flags sorted (but keep enable/disable pairs together).
ASH_PUBLIC_EXPORT extern const char kCustomLauncherPage[];
ASH_PUBLIC_EXPORT extern const char kDisableAppListDismissOnBlur[];
ASH_PUBLIC_EXPORT extern const char kEnableAppList[];
ASH_PUBLIC_EXPORT extern const char kEnableDriveSearchInChromeLauncher[];
ASH_PUBLIC_EXPORT extern const char kDisableDriveSearchInChromeLauncher[];
ASH_PUBLIC_EXPORT extern const char kEnableCrOSActionRecorder[];
ASH_PUBLIC_EXPORT extern const char kCrOSActionRecorderWithHash[];
ASH_PUBLIC_EXPORT extern const char kCrOSActionRecorderWithoutHash[];

bool ASH_PUBLIC_EXPORT IsAppListSyncEnabled();

bool ASH_PUBLIC_EXPORT IsFolderUIEnabled();

// Determines whether the app list should not be dismissed on focus loss.
bool ASH_PUBLIC_EXPORT ShouldNotDismissOnBlur();

}  // namespace switches
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_SWITCHES_H_
