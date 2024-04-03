// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/test_util.h"

#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#else
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/crosapi/test_controller_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace crosapi {

namespace internal {

int GetInterfaceVersionImpl(base::Token interface_uuid) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::LacrosService::Get()->GetInterfaceVersion(interface_uuid);
#else
  auto it = browser_util::GetInterfaceVersions().find(interface_uuid);
  return it == browser_util::GetInterfaceVersions().end() ? -1 : it->second;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace internal

mojom::TestController* GetTestController() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::LacrosService::Get()
      ->GetRemote<mojom::TestController>()
      .get();
#else
  return TestControllerAsh::Get();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

bool AshSupportsCapabilities(const base::flat_set<std::string>& capabilities) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const std::optional<std::vector<std::string>>& ash_capabilities =
      chromeos::BrowserParamsProxy::Get()->AshCapabilities();
  return ash_capabilities &&
         base::ranges::includes(
             base::MakeFlatSet<std::string>(*ash_capabilities), capabilities);
#else
  return base::ranges::includes(
      base::MakeFlatSet<std::string>(browser_util::GetAshCapabilities(),
                                     /*comp=*/{},
                                     /*proj=*/
                                     [](const std::string_view& capability) {
                                       return std::string(capability);
                                     }),
      capabilities);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace crosapi
