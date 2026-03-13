// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_result_type.h"

#include "build/build_config.h"

namespace apps {

LaunchResult ConvertBoolToLaunchResult(bool success) {
  return success ? LaunchResult::kSuccess : LaunchResult::kFailed;
}

}  // namespace apps
