// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_result_type.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace apps {

LaunchResult::LaunchResult() = default;
LaunchResult::~LaunchResult() = default;

LaunchResult::LaunchResult(LaunchResult&& other) = default;

#if BUILDFLAG(IS_CHROMEOS)
LaunchResult ConvertMojomLaunchResultToLaunchResult(
    crosapi::mojom::LaunchResultPtr mojom_launch_result) {
  auto launch_result = LaunchResult();
  if (mojom_launch_result->instance_ids) {
    for (auto token : *mojom_launch_result->instance_ids)
      launch_result.instance_ids.push_back(std::move(token));
  } else {
    launch_result.instance_ids.push_back(
        std::move(mojom_launch_result->instance_id));
  }
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
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace apps
