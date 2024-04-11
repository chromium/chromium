// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/echo/echo_util.h"

#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/check_is_test.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/echo_private_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/echo_private.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos::echo_util {
namespace {

// Performs an explicit conversion from `str` to `base::ok(str)`.
base::ok<std::string> ToExpected(const std::string& str) {
  return base::ok(str);
}

}  // namespace

void GetOobeTimestamp(GetOobeTimestampCallback callback) {
  // NOTE: Ensure that `callback` will run asynchronously.
  callback = base::BindPostTaskToCurrentDefault(std::move(callback));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->echo_private_ash()
        ->GetOobeTimestamp(
            base::BindOnce(&ToExpected).Then(std::move(callback)));
  } else {
    CHECK_IS_TEST();
    std::move(callback).Run(base::ok(std::string()));
  }
#else  // BUILDFLAG(IS_CHROMEOS_ASH)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::EchoPrivate>() &&
      static_cast<uint32_t>(
          lacros_service->GetInterfaceVersion<crosapi::mojom::EchoPrivate>()) >=
          crosapi::mojom::EchoPrivate::kGetOobeTimestampMinVersion) {
    lacros_service->GetRemote<crosapi::mojom::EchoPrivate>()->GetOobeTimestamp(
        base::BindOnce(&ToExpected).Then(std::move(callback)));
  } else {
    std::move(callback).Run(base::unexpected("EchoPrivate unavailable."));
  }
#endif
}

}  // namespace chromeos::echo_util
