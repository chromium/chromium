// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ppapi_utils.h"
#include "chrome/common/pref_names.h"
#include "components/nacl/common/buildflags.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

#if BUILDFLAG(ENABLE_NACL)
bool ShouldNaClBeAllowed() {
#if BUILDFLAG(IS_CHROMEOS)
  // ForceEnabled by policy.
  if (g_browser_process->local_state()->GetBoolean(
          prefs::kNativeClientForceAllowed)) {
    return true;
  }

  // On unmanaged devices we consider NaCl enabled until we will implement a
  // device owner settings to emulate the functionality of the device policy
  // above.
  // TODO(crbug.com/377443982): Modify after device owner settings is
  // implemented.
  BrowserProcessPlatformPart* platform_part =
      g_browser_process->platform_part();
  if (platform_part) {
    policy::BrowserPolicyConnectorAsh* connector =
        platform_part->browser_policy_connector_ash();
    if (connector && !connector->IsDeviceEnterpriseManaged()) {
      return true;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(kNaclAllow);
}
#endif

}  // namespace

BASE_FEATURE(kNaclAllow, "NaclAllow", base::FEATURE_DISABLED_BY_DEFAULT);

void ChromeBrowserMainExtraPartsNaclDeprecation::PostEarlyInitialization() {
#if BUILDFLAG(ENABLE_NACL)
  if (!ShouldNaClBeAllowed()) {
    DisallowNacl();
  }
#endif
}
