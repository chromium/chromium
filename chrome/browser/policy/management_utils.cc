// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/management_utils.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/enterprise_util.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace policy {

bool IsDeviceEnterpriseManaged() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->IsDeviceEnterpriseManaged();
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return base::IsManagedDevice();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsDeviceEnterprisedManaged();
#else
  return false;
#endif
}

bool IsDeviceCloudManaged() {
#if BUILDFLAG(IS_CHROMEOS)
  return IsDeviceEnterpriseManaged();
#else
  return policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().is_valid();
#endif
}

}  // namespace policy
