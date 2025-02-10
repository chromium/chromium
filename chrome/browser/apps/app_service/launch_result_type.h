// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_RESULT_TYPE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_RESULT_TYPE_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/unguessable_token.h"

namespace apps {
// LaunchResult and LaunchCallback can be used in ChromeOS and other
// desktop platforms. So this struct can't be moved to AppPublisher.

struct LaunchResult {
  LaunchResult();
  ~LaunchResult();

  LaunchResult(LaunchResult&& launch_result);
  LaunchResult& operator=(const LaunchResult& launch_result) = delete;
  LaunchResult(const LaunchResult& launch_result) = delete;

  enum class State { kSuccess, kFailed, kFailedDirectoryNotShared };
  explicit LaunchResult(LaunchResult::State state);

  // A single launch event may result in multiple app instances being created.
  std::vector<base::UnguessableToken> instance_ids;

  // Indicates whether the launch attempt was successful or not.
  State state = LaunchResult::State::kFailed;
};

using LaunchCallback = base::OnceCallback<void(LaunchResult&&)>;
using State = LaunchResult::State;

LaunchResult ConvertBoolToLaunchResult(bool success);

bool ConvertLaunchResultToBool(const LaunchResult& result);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_RESULT_TYPE_H_
