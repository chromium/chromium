// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_result_type.h"

#if defined(OS_CHROMEOS)
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace apps {

#if defined(OS_CHROMEOS)
LaunchResult ConvertMojomLaunchResultToLaunchResult(
    crosapi::mojom::LaunchResultPtr mojom_launch_result) {
  auto launch_result = LaunchResult();
  launch_result.instance_id = std::move(mojom_launch_result->instance_id);
  return launch_result;
}

base::OnceCallback<void(crosapi::mojom::LaunchResultPtr)>
LaunchResultToMojomLaunchResultCallback(LaunchCallback callback) {
  return base::BindOnce(
      [](LaunchCallback inner_callback,
         crosapi::mojom::LaunchResultPtr mojom_launch_result) {
        std::move(inner_callback)
            .Run(ConvertMojomLaunchResultToLaunchResult(
                std::move(mojom_launch_result)));
      },
      std::move(callback));
}
#endif  // defined(OS_CHROMEOS)

}  // namespace apps
