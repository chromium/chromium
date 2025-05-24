// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/management/management_util.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"

namespace extensions {

policy::ManagementAuthorityTrustworthiness
GetHigherManagementAuthorityTrustworthiness(Profile* profile) {
  policy::ManagementAuthorityTrustworthiness platform_trustworthiness =
      policy::ManagementServiceFactory::GetForPlatform()
          ->GetManagementAuthorityTrustworthiness();
  policy::ManagementAuthorityTrustworthiness browser_trustworthiness =
      policy::ManagementServiceFactory::GetForProfile(profile)
          ->GetManagementAuthorityTrustworthiness();
  return std::max(platform_trustworthiness, browser_trustworthiness);
}
}  // namespace extensions

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
