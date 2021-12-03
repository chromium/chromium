// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_RESULT_TYPE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_RESULT_TYPE_H_

#include "base/callback_forward.h"
#include "base/unguessable_token.h"

#if defined(OS_CHROMEOS)
#include "chromeos/crosapi/mojom/app_service_types.mojom-forward.h"
#endif  // defined(OS_CHROMEOS)

namespace apps {
// LaunchResult, and LaunchCallback can be used in Chrome Ash, lacros, and other
// desktop platforms. So this struct can't be moved to AppPublisher.

struct LaunchResult {
  base::UnguessableToken instance_id;
};

using LaunchCallback = base::OnceCallback<void(LaunchResult&&)>;

#if defined(OS_CHROMEOS)
LaunchResult ConvertMojomLaunchResultToLaunchResult(
    crosapi::mojom::LaunchResultPtr mojom_launch_result);

base::OnceCallback<void(crosapi::mojom::LaunchResultPtr)>
LaunchResultToMojomLaunchResultCallback(LaunchCallback callback);
#endif  // defined(OS_CHROMEOS)

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_RESULT_TYPE_H_
