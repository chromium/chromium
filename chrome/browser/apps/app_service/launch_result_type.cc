// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_result_type.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace apps {
namespace {
#if BUILDFLAG(IS_CHROMEOS)
LaunchResult::State ConvertMojomLaunchResultStateToLaunchResultState(
    crosapi::mojom::LaunchResultState state) {
  switch (state) {
    case crosapi::mojom::LaunchResultState::kFailed:
      return LaunchResult::State::kFailed;
    case crosapi::mojom::LaunchResultState::kFailedDirectoryNotShared:
      return LaunchResult::State::kFailedDirectoryNotShared;
    case crosapi::mojom::LaunchResultState::kSuccess:
      return LaunchResult::State::kSuccess;
  }
}
crosapi::mojom::LaunchResultState
ConvertLaunchResultStateToMojomLaunchResultState(LaunchResult::State state) {
  switch (state) {
    case LaunchResult::State::kFailed:
      return crosapi::mojom::LaunchResultState::kFailed;
    case LaunchResult::State::kFailedDirectoryNotShared:
      return crosapi::mojom::LaunchResultState::kFailedDirectoryNotShared;
    case LaunchResult::State::kSuccess:
      return crosapi::mojom::LaunchResultState::kSuccess;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)
}  // namespace

LaunchResult::LaunchResult() = default;
LaunchResult::~LaunchResult() = default;

LaunchResult::LaunchResult(LaunchResult&& other) = default;

LaunchResult::LaunchResult(LaunchResult::State state) : state(state) {}

#if BUILDFLAG(IS_CHROMEOS)
LaunchResult ConvertMojomLaunchResultToLaunchResult(
    crosapi::mojom::LaunchResultPtr mojom_launch_result) {
  auto launch_result = LaunchResult();
  if (mojom_launch_result->instance_ids) {
    for (auto token : *mojom_launch_result->instance_ids) {
      launch_result.instance_ids.push_back(std::move(token));
    }
  } else {
    launch_result.instance_ids.push_back(
        std::move(mojom_launch_result->instance_id));
  }
  launch_result.state = ConvertMojomLaunchResultStateToLaunchResultState(
      mojom_launch_result->state);
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

crosapi::mojom::LaunchResultPtr ConvertLaunchResultToMojomLaunchResult(
    LaunchResult&& launch_result) {
  auto mojom_launch_result = crosapi::mojom::LaunchResult::New();
  mojom_launch_result->instance_ids = std::vector<base::UnguessableToken>();
  for (auto& token : launch_result.instance_ids) {
    if (!token.is_empty()) {
      mojom_launch_result->instance_ids->push_back(token);
    }
  }
  // This is a deprecated field, but unfortunately we cannot leave it blank,
  // because the serialization code check-fails if this field
  // is not set. So we just set it to the first of the instance_ids or create
  // a dummy value.
  // Code will always use the instance_ids field over the instance_id, as
  // demonstrated above, so this is safe.
  if (!launch_result.instance_ids.empty()) {
    mojom_launch_result->instance_id =
        mojom_launch_result->instance_ids->front();
  } else {
    mojom_launch_result->instance_id = base::UnguessableToken::Create();
  }
  mojom_launch_result->state =
      ConvertLaunchResultStateToMojomLaunchResultState(launch_result.state);
  return mojom_launch_result;
}

LaunchCallback MojomLaunchResultToLaunchResultCallback(
    base::OnceCallback<void(crosapi::mojom::LaunchResultPtr)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(crosapi::mojom::LaunchResultPtr)>
             inner_callback,
         LaunchResult&& launch_result) {
        std::move(inner_callback)
            .Run(ConvertLaunchResultToMojomLaunchResult(
                std::move(launch_result)));
      },
      std::move(callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

LaunchResult ConvertBoolToLaunchResult(bool success) {
  return success ? LaunchResult(State::kSuccess) : LaunchResult(State::kFailed);
}

bool ConvertLaunchResultToBool(const LaunchResult& result) {
  return result.state == State::kSuccess;
}

}  // namespace apps
