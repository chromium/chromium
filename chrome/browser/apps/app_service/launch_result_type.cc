// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_result_type.h"

#include "build/build_config.h"

namespace apps {

LaunchResult::LaunchResult() = default;
LaunchResult::~LaunchResult() = default;

LaunchResult::LaunchResult(LaunchResult&& other) = default;

LaunchResult::LaunchResult(LaunchResult::State state) : state(state) {}

LaunchResult ConvertBoolToLaunchResult(bool success) {
  return success ? LaunchResult(State::kSuccess) : LaunchResult(State::kFailed);
}

bool ConvertLaunchResultToBool(const LaunchResult& result) {
  return result.state == State::kSuccess;
}

}  // namespace apps
