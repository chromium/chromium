// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_RESULT_TYPE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_RESULT_TYPE_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/app_service_types.mojom-forward.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace apps {
// LaunchResult, and LaunchCallback can be used in Chrome Ash, lacros, and other
// desktop platforms. So this struct can't be moved to AppPublisher.

struct LaunchResult {
  LaunchResult();
  ~LaunchResult();

  LaunchResult(LaunchResult&& launch_result);
  LaunchResult& operator=(const LaunchResult& launch_result) = delete;
  LaunchResult(const LaunchResult& launch_result) = delete;

  // A single launch event may result in multiple app instances being created.
  std::vector<base::UnguessableToken> instance_ids;
};

using LaunchCallback = base::OnceCallback<void(LaunchResult&&)>;

#if BUILDFLAG(IS_CHROMEOS)
LaunchResult ConvertMojomLaunchResultToLaunchResult(
    crosapi::mojom::LaunchResultPtr mojom_launch_result);

base::OnceCallback<void(crosapi::mojom::LaunchResultPtr)>
LaunchResultToMojomLaunchResultCallback(LaunchCallback callback);
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_RESULT_TYPE_H_
