// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/echo/echo_util.h"

#include <optional>
#include <string>
#include <utility>

#include "base/task/bind_post_task.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/report/utils/time_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/echo_private.mojom.h"  // nogncheck
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos::echo_util {
namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Parse the given timestamp from Lacros.
std::optional<base::Time> ParseTime(const std::string& str) {
  base::Time result;
  if (!base::Time::FromUTCString(str.c_str(), &result)) {
    return std::nullopt;
  }
  return {result};
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

void GetOobeTimestamp(GetOobeTimestampCallback callback) {
  // NOTE: Ensure that `callback` will run asynchronously.
  callback = base::BindPostTaskToCurrentDefault(std::move(callback));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::move(callback).Run(ash::report::utils::GetFirstActiveWeek());
#else  // BUILDFLAG(IS_CHROMEOS_ASH)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::EchoPrivate>() &&
      static_cast<uint32_t>(
          lacros_service->GetInterfaceVersion<crosapi::mojom::EchoPrivate>()) >=
          crosapi::mojom::EchoPrivate::kGetOobeTimestampMinVersion) {
    lacros_service->GetRemote<crosapi::mojom::EchoPrivate>()->GetOobeTimestamp(
        base::BindOnce(&ParseTime).Then(std::move(callback)));
  } else {
    std::move(callback).Run(std::nullopt);
  }
#endif
}

}  // namespace chromeos::echo_util
