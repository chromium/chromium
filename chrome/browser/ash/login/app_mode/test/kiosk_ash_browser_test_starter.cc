// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/kiosk_ash_browser_test_starter.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/fake_device_ownership_waiter.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace ash {

bool KioskAshBrowserTestStarter::HasLacrosArgument() {
  return ash_browser_test_starter_.HasLacrosArgument();
}

void KioskAshBrowserTestStarter::PrepareEnvironmentForKioskLacros() {
  CHECK(ash_browser_test_starter_.PrepareEnvironmentForLacros());

  // The `kDisableLacrosKeepAliveForTesting` switch is set by
  // `AshBrowserTestStarter`, but kiosk launch relies on `KeepAlive`, so remove
  // it again.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kDisableLacrosKeepAliveForTesting);
}

void KioskAshBrowserTestStarter::SetLacrosAvailabilityPolicy() {
  DCHECK(HasLacrosArgument());
  policy::PolicyMap policy;
  policy.Set(policy::key::kLacrosAvailability, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(GetLacrosAvailabilityPolicyName(
                 ash::standalone_browser::LacrosAvailability::kLacrosOnly)),
             /*external_data_fetcher=*/nullptr);
  crosapi::browser_util::CacheLacrosAvailability(policy);
}

void KioskAshBrowserTestStarter::SetUpBrowserManager() {
  ash_browser_test_starter_.SetUpBrowserManager();
}

}  // namespace ash
