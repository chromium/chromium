// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_install/app_install_service_ash.h"
#else
#include "chrome/browser/apps/app_service/app_install/app_install_service_lacros.h"
#endif

#include <ostream>

namespace apps {

// static
std::unique_ptr<AppInstallService> AppInstallService::Create(Profile& profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<AppInstallServiceAsh>(profile);
#else
  return std::make_unique<AppInstallServiceLacros>();
#endif
}

AppInstallService::~AppInstallService() = default;

}  // namespace apps
